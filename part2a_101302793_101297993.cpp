#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>

using namespace std;

#define NUM_QUESTIONS 5

struct SharedData {
    int currentStudent;
    int currentExamIndex;
    char rubric[NUM_QUESTIONS];
    int questionStatus[NUM_QUESTIONS]; // 0 = unmarked, 1 = marking, 2 = done
    int stopFlag;
};

void loadRubric(SharedData* shm) {
    ifstream file("rubric.txt");
    int num;
    char comma, letter;

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        file >> num >> comma >> letter;

        // SANITIZE: force printable ASCII
        if (letter < 32 || letter > 126) {
            letter = 'A';
        }

        shm->rubric[i] = letter;
    }

    file.close();
}

void saveRubric(SharedData* shm) {
    ofstream file("rubric.txt");
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        file << (i+1) << ", " << shm->rubric[i] << endl;
    }
    file.close();
}

void loadExam(SharedData* shm) {
    char filename[50];

    // If we've passed exam 20, jump directly to termination exam 9999
    if (shm->currentExamIndex >= 20) {
        sprintf(filename, "exams/exam_9999.txt");
    } else {
        sprintf(filename, "exams/exam_%04d.txt", shm->currentExamIndex + 1);
    }

    ifstream file(filename);
    if (!file.is_open()) {
        perror("Error opening exam file");
        return;
    }

    file >> shm->currentStudent;
    file.close();

    for (int i = 0; i < NUM_QUESTIONS; i++)
        shm->questionStatus[i] = 0;
}

void TAprocess(int id, SharedData* shm) {
    srand(time(NULL) + id);

    while (true) {
        if (shm->stopFlag == 1)
            break;

        // Review rubric
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            usleep((500 + rand() % 500) * 1000);

            if (rand() % 4 == 0) { // random correction
                char oldVal = shm->rubric[i];

                // SAFE ASCII CLAMP
                if (shm->rubric[i] >= 126) {   // '~' is last printable ASCII
                    shm->rubric[i] = 'A';
                } else {
                    shm->rubric[i]++;
                }

                cout << "TA " << id << " corrected rubric Q" << i+1
                     << " from " << oldVal << " to " << shm->rubric[i] << endl;

                saveRubric(shm);
            }
        }

        // Mark a question
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (shm->questionStatus[i] == 0) {
                shm->questionStatus[i] = 1;
                sleep(1 + rand() % 2);

                shm->questionStatus[i] = 2;
                cout << "TA " << id << " marked Question "
                     << (i+1) << " for Student "
                     << shm->currentStudent << endl;
                break;
            }
        }

        // Check if all questions are done
        bool done = true;
        for (int i = 0; i < NUM_QUESTIONS; i++)
            if (shm->questionStatus[i] != 2)
                done = false;

        if (done) {
            shm->currentExamIndex++;
            loadExam(shm);

            if (shm->currentStudent == 9999) {
                shm->stopFlag = 1;
                cout << "TA " << id << " detected termination exam." << endl;
                break;
            } else {
                cout << "Loading next exam: Student "
                     << shm->currentStudent << endl;
            }
        }
    }

    cout << "TA " << id << " exiting." << endl;
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "Usage: ./part2a <number_of_TAs>" << endl;
        return 1;
    }

    int numTAs = atoi(argv[1]);
    if (numTAs < 2) {
        cout << "At least 2 TAs required." << endl;
        return 1;
    }

    int shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    SharedData* shm = (SharedData*) shmat(shmid, NULL, 0);

    shm->currentExamIndex = 0;
    shm->stopFlag = 0;

    loadRubric(shm);
    loadExam(shm);

    for (int i = 0; i < numTAs; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            TAprocess(i+1, shm);
        }
    }

    for (int i = 0; i < numTAs; i++)
        wait(NULL);

    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);

    cout << "All TAs finished." << endl;
    return 0;
}
