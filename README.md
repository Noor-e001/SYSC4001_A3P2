# SYSC4001_A3P2

# Concurrent Processes with Shared Memory and Semaphores

Overview:
This repository contains the implementation for Part 2 of Assignment 3.
It includes:
- Part 2.a: Concurrent TA processes using shared memory without semaphores
- Part 2.b: The same system using semaphores for synchronization
- Part 2.c: A report analyzing deadlock, livelock, and execution order
The program simulates multiple Teaching Assistants (TAs) marking exams at the same time.

Files in This Repository:
- part2a.cpp: Unsynchronized concurrent version (race conditions allowed)
- part2b.cpp: Semaphore-based synchronized version
- CMakeLists.txt: Build configuration
- rubric.txt: Initial rubric file
- exams: Folder containing:
    - exam_0001.txt to exam_0020.txt
    - exam_9999.txt: termination file
- reportPartC.pdf: Deadlock/livelock analysis report
- README.md: This file

System Requirements:
- Linux or WSL (Ubuntu)
- CMake
- GCC / G++
- POSIX semaphores support
These programs were built and tested using CLion with WSL.

How to Compile:

From the project root directory:
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug

This will build both:
part2a
part2b

How to Run:
Navigate to the build folder:
cd cmake-build-debug

Run Part 2.a (No Semaphores)
./part2a 3

Run Part 2.b (With Semaphores)
./part2b 3

The number 3 represents the number of Teaching Assistants (TAs).
You can use any value ≥ 2.

Program Behavior:
Each TA process will:
1. Review the rubric and randomly correct entries.
2. Mark one question per exam.
3. After all 5 questions are marked, one TA loads the next exam.
4. The system stops when the student number 9999 is reached.
Differences Between Versions:
Part 2.a
- No synchronization
- Race conditions may occur
- Multiple TAs may act on the same question

Part 2.b
- Uses semaphores for synchronization
- Only one TA can:
    - Modify the rubric at a time
    - Mark a given question
    - Load the next exam
- No race conditions

Output:
Both programs print:
- Which TA is correcting the rubric
- Which question is being marked
- When a new exam is loaded
- When the termination exam is detected
- When each TA exits
- A final completion message

Part 2.c Report:
The file reportPartC.pdf contains:
- Analysis of Part 2.a behavior
- Analysis of Part 2.b behavior
- Deadlock and livelock discussion
- Execution order discussion
- Final conclusions

Notes:
The terminating exam must be named:
exam_9999.txt
The rubric file must exist as:
rubric.txt
The exams directory must be in the same working directory as the executables.
