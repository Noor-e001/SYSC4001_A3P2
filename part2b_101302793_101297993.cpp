#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <cstdlib>
#include <ctime>
#include <semaphore.h>

using namespace std;

#define NUM_QUESTIONS 5

struct SharedData {
    int currentStudent;
    int currentExamIndex;
    char rubric[NUM_QUESTIONS];
    int questionStatus[NUM_QUESTIONS]; // 0 = unmarked, 1 = marking, 2 = done
    int stopFlag;

    // Semaphores
    sem_t rubric_mutex;   // protects rubric and rubric file
    sem_t exam_mutex;     // protects questionStatus and current exam state
    sem_t loader_mutex;   // makes sure only one TA loads the next exam
    sem_t print_mutex;    // cleans up console output
};

void loadRubric(SharedData* shm) {
    ifstream file("rubric.txt");
    int num;
    char comma, letter;

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        file >> num >> comma >> letter;

        // Sanitize: make sure we start with printable characters
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
        shm->currentStudent = 9999;
        shm->stopFlag = 1;
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

        // Check for global stop
        if (shm->stopFlag == 1) {
            sem_wait(&shm->print_mutex);
            cout << "TA " << id << " exiting (stop flag set)." << endl;
            sem_post(&shm->print_mutex);
            exit(0);
        }

        // Review rubric (with synchronization)
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            usleep((500 + rand() % 500) * 1000);

            if (rand() % 4 == 0) { // random correction
                sem_wait(&shm->rubric_mutex);

                char oldVal = shm->rubric[i];

                // Safe ASCII clamp: keep printable range and wrap after '~'
                if (shm->rubric[i] >= 126) {   // '~' is last printable ASCII
                    shm->rubric[i] = 'A';
                } else {
                    shm->rubric[i]++;
                }

                sem_wait(&shm->print_mutex);
                cout << "TA " << id << " corrected rubric Q" << i+1
                     << " from " << oldVal << " to " << shm->rubric[i] << endl;
                sem_post(&shm->print_mutex);

                saveRubric(shm);
                sem_post(&shm->rubric_mutex);
            }
        }

        //  Mark a question (with synchronization)
        int questionToMark = -1;
        int studentCopy = -1;

        // Pick an unmarked question safely
        sem_wait(&shm->exam_mutex);
        if (shm->stopFlag == 1) {
            sem_post(&shm->exam_mutex);
            continue;
        }

        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (shm->questionStatus[i] == 0) {
                shm->questionStatus[i] = 1; // marking
                questionToMark = i;
                studentCopy = shm->currentStudent;
                break;
            }
        }
        sem_post(&shm->exam_mutex);

        // If there was a question to mark, simulate marking
        if (questionToMark != -1) {
            sleep(1 + rand() % 2);

            sem_wait(&shm->exam_mutex);
            shm->questionStatus[questionToMark] = 2; // done
            sem_post(&shm->exam_mutex);

            sem_wait(&shm->print_mutex);
            cout << "TA " << id << " marked Question "
                 << (questionToMark + 1) << " for Student "
                 << studentCopy << endl;
            sem_post(&shm->print_mutex);
        }

        // Check if exam is fully marked and possibly load next
        bool done = true;

        sem_wait(&shm->exam_mutex);
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (shm->questionStatus[i] != 2) {
                done = false;
                break;
            }
        }
        sem_post(&shm->exam_mutex);

        if (done) {
            // Only one TA at a time should decide to load the next exam
            sem_wait(&shm->loader_mutex);

            // Double-check after acquiring loader lock
            bool doneAgain = true;

            sem_wait(&shm->exam_mutex);
            for (int i = 0; i < NUM_QUESTIONS; i++) {
                if (shm->questionStatus[i] != 2) {
                    doneAgain = false;
                    break;
                }
            }
            int currentStudent = shm->currentStudent;
            sem_post(&shm->exam_mutex);

            if (doneAgain && shm->stopFlag == 0) {
                shm->currentExamIndex++;
                loadExam(shm);

                if (shm->currentStudent == 9999) {
                    shm->stopFlag = 1;
                    sem_wait(&shm->print_mutex);
                    cout << "TA " << id << " detected termination exam." << endl;
                    sem_post(&shm->print_mutex);
                } else {
                    sem_wait(&shm->print_mutex);
                    cout << "Loading next exam: Student "
                         << shm->currentStudent << endl;
                    sem_post(&shm->print_mutex);
                }
            }

            sem_post(&shm->loader_mutex);
        }

        // If stopFlag got set during this iteration, exit on next loop
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "Usage: ./part2b <number_of_TAs>" << endl;
        return 1;
    }

    int numTAs = atoi(argv[1]);
    if (numTAs < 2) {
        cout << "At least 2 TAs required." << endl;
        return 1;
    }

    // Create shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        return 1;
    }

    SharedData* shm = (SharedData*) shmat(shmid, NULL, 0);
    if (shm == (void*) -1) {
        perror("shmat");
        return 1;
    }

    // Initialize shared data
    shm->currentExamIndex = 0;
    shm->stopFlag = 0;

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        shm->questionStatus[i] = 0;
    }

    loadRubric(shm);
    loadExam(shm);

    // Initialize semaphores (pshared = 1 means shared between processes)
    sem_init(&shm->rubric_mutex, 1, 1);
    sem_init(&shm->exam_mutex,   1, 1);
    sem_init(&shm->loader_mutex, 1, 1);
    sem_init(&shm->print_mutex,  1, 1);

    // Fork TA processes
    for (int i = 0; i < numTAs; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            TAprocess(i + 1, shm);
        }
    }

    // Parent waits for all TAs
    for (int i = 0; i < numTAs; i++) {
        wait(NULL);
    }

    // Destroy semaphores
    sem_destroy(&shm->rubric_mutex);
    sem_destroy(&shm->exam_mutex);
    sem_destroy(&shm->loader_mutex);
    sem_destroy(&shm->print_mutex);

    // Detach and remove shared memory
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);

    cout << "All TAs finished (Part 2.b with semaphores)." << endl;
    return 0;
}
