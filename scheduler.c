#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Headers as needed

typedef enum { false, true } bool;        // Allows boolean types in C

/* Defines a job struct */
typedef struct Process {
    uint32_t A;                         // A: Arrival time of the process
    uint32_t B;                         // B: Upper Bound of CPU burst times of the given random integer list
    uint32_t C;                         // C: Total CPU time required
    uint32_t M;                         // M: Multiplier of CPU burst time
    uint32_t processID;                 // The process ID given upon input read

    uint8_t status;                     // 0 is unstarted, 1 is ready, 2 is running, 3 is blocked, 4 is terminated

    int32_t finishingTime;              // The cycle when the the process finishes (initially -1)
    uint32_t currentCPUTimeRun;         // The amount of time the process has already run (time in running state)
    uint32_t currentIOBlockedTime;      // The amount of time the process has been IO blocked (time in blocked state)
    uint32_t currentWaitingTime;        // The amount of time spent waiting to be run (time in ready state)

    uint32_t IOBurst;                   // The amount of time until the process finishes being blocked
    uint32_t CPUBurst;                  // The CPU availability of the process (has to be > 1 to move to running)

    int32_t quantum;                    // Used for schedulers that utilise pre-emption

    bool isFirstTimeRunning;            // Used to check when to calculate the CPU burst when it hits running mode

    struct Process* nextInBlockedList;  // A pointer to the next process available in the blocked list
    struct Process* nextInReadyQueue;   // A pointer to the next process available in the ready queue
    struct Process* nextInReadySuspendedQueue; // A pointer to the next process available in the ready suspended queue
} _process;


uint32_t CURRENT_CYCLE = 0;             // The current cycle that each process is on
uint32_t TOTAL_CREATED_PROCESSES = 0;   // The total number of processes constructed
uint32_t TOTAL_STARTED_PROCESSES = 0;   // The total number of processes that have started being simulated
uint32_t TOTAL_FINISHED_PROCESSES = 0;  // The total number of processes that have finished running
uint32_t TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED = 0; // The total cycles in the blocked state

const char* RANDOM_NUMBER_FILE_NAME = "random-numbers";
const uint32_t SEED_VALUE = 200;  // Seed value for reading from file

// Additional variables as needed


/**
 * Reads a random non-negative integer X from a file with a given line named random-numbers (in the current directory)
 */
uint32_t getRandNumFromFile(uint32_t line, FILE* random_num_file_ptr) {
    uint32_t end, loop;
    char str[512] = "";

    rewind(random_num_file_ptr); // reset to be beginning
    for (end = loop = 0; loop < line; ++loop) {
        if (0 == fgets(str, sizeof(str), random_num_file_ptr)) { //include '\n'
            end = 1;  //can't input (EOF)
            break;
        }
    }
    if (!end) {
        return (uint32_t)atoi(str);
    }

    // fail-safe return
    return (uint32_t)1804289383;
}



/**
 * Reads a random non-negative integer X from a file named random-numbers.
 * Returns the CPU Burst: : 1 + (random-number-from-file % upper_bound)
 */
uint32_t randomOS(uint32_t upper_bound, uint32_t process_indx, FILE* random_num_file_ptr)
{
    char str[20];

    uint32_t unsigned_rand_int = (uint32_t)getRandNumFromFile(SEED_VALUE + process_indx, random_num_file_ptr);
    uint32_t returnValue = 1 + (unsigned_rand_int % upper_bound);

    return returnValue;
}


/********************* SOME PRINTING HELPERS *********************/


/**
 * Prints to standard output the original input
 * process_list is the original processes inputted (in array form)
 */
void printStart(_process process_list[])
{
    printf("The original input was: %i", TOTAL_CREATED_PROCESSES);

    uint32_t i = 0;
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        printf(" ( %i %i %i %i)", process_list[i].A, process_list[i].B,
            process_list[i].C, process_list[i].M);
    }
    printf("\n");
}

/**
 * Prints to standard output the final output
 * finished_process_list is the terminated processes (in array form) in the order they each finished in.
 */
void printFinal(_process finished_process_list[])
{
    printf("The (sorted) input is: %i", TOTAL_CREATED_PROCESSES);

    uint32_t i = 0;
    for (; i < TOTAL_FINISHED_PROCESSES; ++i)
    {
        printf(" ( %i %i %i %i)", finished_process_list[i].A, finished_process_list[i].B,
            finished_process_list[i].C, finished_process_list[i].M);
    }
    printf("\n");
} // End of the print final function

/**
 * Prints out specifics for each process.
 * @param process_list The original processes inputted, in array form
 */
void printProcessSpecifics(_process process_list[])
{
    uint32_t i = 0;
    printf("\n");
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        printf("Process %i:\n", process_list[i].processID);
        printf("\t(A,B,C,M) = (%i,%i,%i,%i)\n", process_list[i].A, process_list[i].B,
            process_list[i].C, process_list[i].M);
        printf("\tFinishing time: %i\n", process_list[i].finishingTime);
        printf("\tTurnaround time: %i\n", process_list[i].finishingTime - process_list[i].A);
        printf("\tI/O time: %i\n", process_list[i].currentIOBlockedTime);
        printf("\tWaiting time: %i\n", process_list[i].currentWaitingTime);
        printf("\n");
    }
} // End of the print process specifics function

/**
 * Prints out the summary data
 * process_list The original processes inputted, in array form
 */
void printSummaryData(_process process_list[])
{
    uint32_t i = 0;
    double total_amount_of_time_utilizing_cpu = 0.0;
    double total_amount_of_time_io_blocked = 0.0;
    double total_amount_of_time_spent_waiting = 0.0;
    double total_turnaround_time = 0.0;
    uint32_t final_finishing_time = CURRENT_CYCLE;
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        total_amount_of_time_utilizing_cpu += process_list[i].currentCPUTimeRun;
        total_amount_of_time_io_blocked += process_list[i].currentIOBlockedTime;
        total_amount_of_time_spent_waiting += process_list[i].currentWaitingTime;
        total_turnaround_time += (process_list[i].finishingTime - process_list[i].A);
    }

    // Calculates the CPU utilisation
    double cpu_util = total_amount_of_time_utilizing_cpu / final_finishing_time;

    // Calculates the IO utilisation
    double io_util = (double)TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED / final_finishing_time;

    // Calculates the throughput (Number of processes over the final finishing time times 100)
    double throughput = 100 * ((double)TOTAL_CREATED_PROCESSES / final_finishing_time);

    // Calculates the average turnaround time
    double avg_turnaround_time = total_turnaround_time / TOTAL_CREATED_PROCESSES;

    // Calculates the average waiting time
    double avg_waiting_time = total_amount_of_time_spent_waiting / TOTAL_CREATED_PROCESSES;

    printf("Summary Data:\n");
    printf("\tFinishing time: %i\n", final_finishing_time);
    printf("\tCPU Utilisation: %6f\n", cpu_util);
    printf("\tI/O Utilisation: %6f\n", io_util);
    printf("\tThroughput: %6f processes per hundred cycles\n", throughput);
    printf("\tAverage turnaround time: %6f\n", avg_turnaround_time);
    printf("\tAverage waiting time: %6f\n", avg_waiting_time);
} // End of the print summary data function

// Function to read a process from the file
void readProcess(FILE* file, _process* process)
{
    char ch;
    // Read up until the opening parenthesis
    while (fscanf(file, "%c", &ch) && ch != '(');
    // Read the quadruple numbers
    fscanf(file, "%d %d %d %d", &process->A, &process->B, &process->C, &process->M);
    // Read up until the closing parenthesis
    while (fscanf(file, "%c", &ch) && ch != ')');
}

// Function for initializing a process before simulation
void initializeProcess(_process* process)
{
    process->status = 1;

    process->finishingTime = -1;
    process->currentCPUTimeRun = 0;
    process->currentIOBlockedTime = 0;
    process->currentWaitingTime = 0;

    process->IOBurst = 0;
    process->CPUBurst = 0;

    process->quantum = 2;

    process->isFirstTimeRunning = true;

    process->nextInBlockedList = NULL;
    process->nextInReadyQueue = NULL;
    process->nextInReadySuspendedQueue = NULL;
}

// Obtain burst times upon isFirstTimeRunning
void obtainBurstTimes(_process* process, FILE* randNumFile)
{
    process->CPUBurst = randomOS(process->B, process->processID, randNumFile);
    process->IOBurst = process->CPUBurst * process->M;
}

// Returns 1 if all processes have terminated, 0 otherwise
int allTerminated(_process process_list[])
{
    int terminated = 1;
    for (int i = 0; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        if (process_list[i].status != 4) terminated = 0;
    }
    return terminated;
}

// Increments values for a process based on its status
void cycle(_process* process)
{
    switch (process->status)
    {
    case 1:
        ++process->currentWaitingTime;
        break;
    case 2:
        ++process->currentCPUTimeRun;
        break;
    case 3:
        ++process->currentIOBlockedTime;
        break;
    case 4:
        break;
    }
}

// Resets global variables
void initializeGlobals(void)
{
    CURRENT_CYCLE = 0;
    TOTAL_STARTED_PROCESSES = 0;
    TOTAL_FINISHED_PROCESSES = 0;
    TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED = 0;
}

// Returns 1 if a process should terminate, 0 otherwise
int hasTerminated(_process* process) { return process->currentCPUTimeRun == process->C; }

// Updates all states and globals when a process terminates
void terminate(_process* process)
{
    process->status = 4;
    process->finishingTime = CURRENT_CYCLE;
    ++TOTAL_FINISHED_PROCESSES;
    TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED += process->currentIOBlockedTime;
}

// Returns 1 if a running process should be blocked, 0 otherwise
int hasBlocked(_process* process) { return ((process->status == 2) && (process->currentCPUTimeRun) && (process->currentCPUTimeRun % process->CPUBurst == 0)); }

// Returns 1 if a blocked process has finished its I/O time, 0 otherwise
int hasFinishedIO(_process* process) { return ((process->status == 3) && (process->currentIOBlockedTime) && (process->currentIOBlockedTime % process->IOBurst == 0)); }

// Returns 1 if a non-running process has arrived on this cycle, 0 otherwise
int hasArrived(_process* process) { return ((process->currentWaitingTime == 1) && (CURRENT_CYCLE == process->A + 1)); }

// Determines and sets the queue order for a given process that has been deemed ready
void determineQueue(_process* processToRun, _process* currentProcess, int numReady, int newlyReady)
{
    _process* pTemp = NULL;

    if (hasArrived(currentProcess)) // need special case for arriving jobs to ensure FCFS
    {
        if (numReady == 2)
        {
            pTemp = processToRun->nextInReadyQueue;
            processToRun->nextInReadyQueue = currentProcess;
            processToRun->nextInReadyQueue->nextInReadyQueue = pTemp;
        }
        else processToRun->nextInReadyQueue = currentProcess;
    }
    else if (numReady == 2)
    {
        if (newlyReady == 2) // correct ready queue order, if necessary (when two jobs have same priority)
        {
            pTemp = processToRun->nextInReadyQueue;
            if (pTemp->A > currentProcess->A) // first sort by arrival time
            {
                processToRun->nextInReadyQueue = currentProcess;
                processToRun->nextInReadyQueue->nextInReadyQueue = pTemp;
            }
            else if ((pTemp->A == currentProcess->A) && (pTemp->processID > currentProcess->processID)) // if same arrival time, sort by order read from file
            {
                processToRun->nextInReadyQueue = currentProcess;
                processToRun->nextInReadyQueue->nextInReadyQueue = pTemp;
            }
            else processToRun->nextInReadyQueue->nextInReadyQueue = currentProcess;
        }
        else processToRun->nextInReadyQueue->nextInReadyQueue = currentProcess;
    }
    else processToRun->nextInReadyQueue = currentProcess;
}

// Determines and sets the queue order for a given process that has been deemed ready, according to Shortest Job First Algorithm
void determineSJF(_process* processToRun, _process* currentProcess, int numReady, int newlyReady)
{
    _process* pTemp = NULL;
  
    if (numReady == 2)
    {
        int remainingNextCPUTime = processToRun->nextInReadyQueue->C - processToRun->nextInReadyQueue->currentCPUTimeRun;
        int remainingNewCPUTime = currentProcess->C - currentProcess->currentCPUTimeRun;

        if (remainingNextCPUTime == remainingNewCPUTime) // if the new job and the currently queued job have the same remaining CPU time
        {
            if (newlyReady == 2) // correct ready queue order, if necessary (when two jobs have same priority)
            {
                pTemp = processToRun->nextInReadyQueue;
                if (pTemp->A > currentProcess->A) // first sort by arrival time
                {
                    processToRun->nextInReadyQueue = currentProcess;
                    processToRun->nextInReadyQueue->nextInReadyQueue = pTemp;
                }
                else if ((pTemp->A == currentProcess->A) && (pTemp->processID > currentProcess->processID)) // if same arrival time, sort by order read from file
                {
                    processToRun->nextInReadyQueue = currentProcess;
                    processToRun->nextInReadyQueue->nextInReadyQueue = pTemp;
                }
                else processToRun->nextInReadyQueue->nextInReadyQueue = currentProcess;
            }
            else processToRun->nextInReadyQueue->nextInReadyQueue = currentProcess;
        }
        else if (remainingNextCPUTime > remainingNewCPUTime) // if the new job has a shorter remaining CPU time than the currently queued job
        {
            pTemp = processToRun->nextInReadyQueue;
            processToRun->nextInReadyQueue = currentProcess;
            processToRun->nextInReadyQueue->nextInReadyQueue = pTemp;
        }
        else processToRun->nextInReadyQueue->nextInReadyQueue = currentProcess; // new job has a longer remaining CPU time
    }
    else processToRun->nextInReadyQueue = currentProcess;
}


/**
 * The magic starts from here
 */
int main(int argc, char* argv[])
{
    uint32_t total_num_of_process;               // Read from the file -- number of process to create
    // Other variables
    _process* processToRun; // The process that is to run
    _process* currentProcess; // The current process whose state is under change
    FILE* randNumFile = fopen(RANDOM_NUMBER_FILE_NAME, "r"); // random numbers file
    int newlyReady, numReady = 0;

    // Write code for your shiny scheduler

    /*
        THIS SCHEDULER HAS ONLY BEEN TESTED WITH UP TO THREE PROCESSES.
        GREATER THAN THREE PROCESSES MAY BREAK THE SIMULATIONS.
    */

    // READING PROCESSES FROM FILE
    FILE* process_file = fopen(argv[1], "r"); // open file
    fscanf(process_file, "%d", &total_num_of_process); // read num of processes
    TOTAL_CREATED_PROCESSES = total_num_of_process;

    _process* process_list = (_process*)malloc(total_num_of_process * sizeof(_process)); // Creates a container for all processes

    for (int i = 0; i < total_num_of_process; ++i) // loop 'total_num_of_process' times
    {
        readProcess(process_file, &process_list[i]); // read all processes
        process_list[i].processID = i;
        initializeProcess(&process_list[i]);
    }
    fclose(process_file); // close the file


    ///////////////////////// FIRST COME FIRST SERVE /////////////////////////

    printf("\n######################### START OF FIRST COME FIRST SERVE #########################\n");
    printStart(process_list);

    processToRun = &process_list[0];
    for (int i = 1; i < TOTAL_CREATED_PROCESSES; ++i) // calculate the first process that will run
    {
        if (process_list[i].A < processToRun->A) processToRun = &process_list[i]; // earliest arrival time + smallest processID (due to iteration of list)
        ++TOTAL_STARTED_PROCESSES;
    }

    while (!allTerminated(process_list))
    {
        ++CURRENT_CYCLE;
        newlyReady = 0;
        if (processToRun)
        {
            if (processToRun->isFirstTimeRunning == true)
            {
                obtainBurstTimes(processToRun, randNumFile);
                processToRun->isFirstTimeRunning = false;
            }
            if (processToRun->status == 4) processToRun = NULL; // don't run a terminated process
            else processToRun->status = 2;
        }

        for (int i = 0; i < TOTAL_CREATED_PROCESSES; ++i)
        {
            currentProcess = &process_list[i];

            if (currentProcess->status == 4) continue; // if a process has terminated, ignore it
            if (CURRENT_CYCLE <= currentProcess->A) continue; // if a process has not "arrived" yet, ignore it

            cycle(currentProcess);

            // check if should be terminated
            if (hasTerminated(currentProcess))
            {
                terminate(currentProcess);
                continue;
            }

            // check if should be blocked
            if (hasBlocked(currentProcess))
            {
                currentProcess->status = 3;
                continue;
            }

            // check if should be ready
            if (hasFinishedIO(currentProcess) || hasArrived(currentProcess))
            {
                currentProcess->status = 1;
        
                if (processToRun)
                {
                    ++newlyReady;
                    ++numReady;
                    determineQueue(processToRun, currentProcess, numReady, newlyReady);
                }
                else
                {
                    processToRun = currentProcess;
                    processToRun->status = 2; // bypass the condition below that would change processToRun
                }
                continue;
            }
        }

        // get next processToRun, if necessary
        if (processToRun)
        {
            if (processToRun->status != 2) // don't switch to a terminated process
            {
                processToRun = processToRun->nextInReadyQueue;
                if (numReady) --numReady;
            }
        }
    }

    printFinal(process_list);
    printf("\nThe scheduling algorithm used was First Come First Serve\n");
    printProcessSpecifics(process_list);
    printSummaryData(process_list);
    printf("\n######################### END OF FIRST COME FIRST SERVE #########################\n");


    ///////////////////////// ROUND ROBIN /////////////////////////

    for (int i = 0; i < TOTAL_CREATED_PROCESSES; ++i) initializeProcess(&process_list[i]); // reset everything for next simulation
    initializeGlobals();
    processToRun = NULL;
    currentProcess = NULL;
    numReady = 0;
    newlyReady = 0;
    int consecutiveCyclesRunning = 0; // NEW FOR RR - keeps track of consecutive running cycles for a process, used to compare against time slice

    printf("\n######################### START OF ROUND ROBIN #########################\n");
    printStart(process_list);

    processToRun = &process_list[0];
    for (int i = 1; i < TOTAL_CREATED_PROCESSES; ++i) // calculate the first process that will run
    {
        if (process_list[i].A < processToRun->A) processToRun = &process_list[i]; // earliest arrival time + smallest processID (due to iteration of list)
        ++TOTAL_STARTED_PROCESSES;
    }

    while (!allTerminated(process_list))
    {
        ++CURRENT_CYCLE;
        newlyReady = 0;
        if (processToRun)
        {
            if (processToRun->isFirstTimeRunning == true)
            {
                obtainBurstTimes(processToRun, randNumFile);
                processToRun->isFirstTimeRunning = false;
            }
            if (processToRun->status == 4) processToRun = NULL; // don't run a terminated process
            else
            {
                processToRun->status = 2;
                ++consecutiveCyclesRunning;
            }
        }

        for (int i = 0; i < TOTAL_CREATED_PROCESSES; ++i)
        {
            currentProcess = &process_list[i];

            if (currentProcess->status == 4) continue; // if a process has terminated, ignore it
            if (CURRENT_CYCLE <= currentProcess->A) continue; // if a process has not "arrived" yet, ignore it

            cycle(currentProcess);

            // check if should be terminated
            if (hasTerminated(currentProcess))
            {
                terminate(currentProcess);
                continue;
            }

            // check if should be blocked
            if (hasBlocked(currentProcess) || 
                consecutiveCyclesRunning >= currentProcess->quantum) // NEW FOR RR - if a process has been running for the time slice, block it
            {
                currentProcess->status = 3;
                continue;
            }

            // check if should be ready
            if (hasFinishedIO(currentProcess) || hasArrived(currentProcess))
            {
                currentProcess->status = 1;

                if (processToRun)
                {
                    ++newlyReady;
                    ++numReady;
                    determineQueue(processToRun, currentProcess, numReady, newlyReady);
                }
                else
                {
                    processToRun = currentProcess;
                    processToRun->status = 2; // bypass the condition below that would change processToRun
                    consecutiveCyclesRunning = 0; // NEW FOR RR - reset consecutive cycles counter
                }
                continue;
            }
        }

        // get next processToRun, if necessary
        if (processToRun)
        {
            if (processToRun->status != 2) // don't switch to a terminated process
            {
                processToRun = processToRun->nextInReadyQueue;
                consecutiveCyclesRunning = 0; // NEW FOR RR - reset consecutive cycles counter
                if (numReady) --numReady;
            }
        }
    }

    printFinal(process_list);
    printf("\nThe scheduling algorithm used was Round Robin\n");
    printProcessSpecifics(process_list);
    printSummaryData(process_list);
    printf("\n######################### END OF ROUND ROBIN #########################\n");


    ///////////////////////// SHORTEST JOB FIRST /////////////////////////

    for (int i = 0; i < TOTAL_CREATED_PROCESSES; ++i) initializeProcess(&process_list[i]); // reset everything for next simulation
    initializeGlobals();
    processToRun = NULL;
    currentProcess = NULL;
    numReady = 0;
    newlyReady = 0;

    printf("\n######################### START OF SHORTEST JOB FIRST #########################\n");
    printStart(process_list);

    processToRun = &process_list[0];
    for (int i = 1; i < TOTAL_CREATED_PROCESSES; ++i) // calculate the first process that will run
    {
        if (process_list[i].C < processToRun->C) processToRun = &process_list[i]; // NEW FOR SJF - shortest CPU time + smallest processID 
        ++TOTAL_STARTED_PROCESSES;
    }

    while (!allTerminated(process_list))
    {
        ++CURRENT_CYCLE;
        newlyReady = 0;
        if (processToRun)
        {
            if (processToRun->isFirstTimeRunning == true)
            {
                obtainBurstTimes(processToRun, randNumFile);
                processToRun->isFirstTimeRunning = false;
            }
            if (processToRun->status == 4) processToRun = NULL; // don't run a terminated process
            else processToRun->status = 2;
        }

        for (int i = 0; i < TOTAL_CREATED_PROCESSES; ++i)
        {
            currentProcess = &process_list[i];

            if (currentProcess->status == 4) continue; // if a process has terminated, ignore it
            if (CURRENT_CYCLE <= currentProcess->A) continue; // if a process has not "arrived" yet, ignore it

            cycle(currentProcess);

            // check if should be terminated
            if (hasTerminated(currentProcess))
            {
                terminate(currentProcess);
                continue;
            }

            // check if should be blocked
            if (hasBlocked(currentProcess))
            {
                currentProcess->status = 3;
                continue;
            }

            // check if should be ready
            if (hasFinishedIO(currentProcess) || hasArrived(currentProcess))
            {
                currentProcess->status = 1;

                if (processToRun)
                {
                    ++newlyReady;
                    ++numReady;
                    determineSJF(processToRun, currentProcess, numReady, newlyReady); // NEW FOR SJF - use new algorithm based on remaining CPU time
                }
                else
                {
                    processToRun = currentProcess;
                    processToRun->status = 2; // bypass the condition below that would change processToRun
                }
                continue;
            }
        }

        // get next processToRun, if necessary
        if (processToRun)
        {
            if (processToRun->status != 2) // don't switch to a terminated process
            {
                processToRun = processToRun->nextInReadyQueue;
                if (numReady) --numReady;
            }
        }
    }

    printFinal(process_list);
    printf("\nThe scheduling algorithm used was Shortest Job First\n");
    printProcessSpecifics(process_list);
    printSummaryData(process_list);
    printf("\n######################### END OF SHORTEST JOB FIRST #########################\n");


    fclose(randNumFile);


    return 0;
}