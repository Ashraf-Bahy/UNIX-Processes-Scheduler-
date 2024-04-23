#include "headers.h"
#include "PriorityQueue.h"
#include "math.h"

int time;
int algorithm;
int msgq_id;

// Statistics
float totalWTA = 0;
float totalWTA2 = 0; // sum of WTA^2 to calculate the standard deviation
int totalWaitingTime = 0;
int totalProcesses = 0;
int totalExecutionTime = 0;
FILE *logFile;

Process **PCBTable;
Process *runningProcess;
PriorityQueuePointer readyQueue;

typedef struct msgbuff
{
    long mtype;
    Process msg_process;
} msgbuff;

void switchProcess(Process **runningProcess, Process *p);
void SRTN();
void HPF(PriorityQueuePointer readyQueue);
void processFinishedHandler(int signum);
void deleteProcess();
void RR(PriorityQueuePointer readyQueue, int timeQuantum);
void receiveProcess(int signum);

void writeToLogFile(int state); // 0 -> runs, 1 -> stops, 2 -> finishes
void createPerfFile();

int main(int argc, char *argv[])
{
    signal(SIGCHLD, processFinishedHandler);
    signal(SIGUSR2, receiveProcess);

    initClk();
    logFile = fopen("scheduler.log", "a");
    time = -1;
    // TODO implement the scheduler :)
    int numberOfProcesses = atoi(argv[3]);
    algorithm = atoi(argv[1]);
    int timeQuantum = atoi(argv[2]); // 1 -> HPF, 2 -> SRTN, 3 -> RR
    runningProcess = NULL;
    PCBTable = malloc(numberOfProcesses * sizeof(Process *));
    readyQueue = priority_queue_init();

    key_t key_id;

    key_id = ftok("pgen_sch_keyfile", 65);
    msgq_id = msgget(key_id, 0666 | IPC_CREAT);

    if (msgq_id == -1)
    {
        perror("Error in creating the queue");
        exit(-1);
    }

    switch (algorithm)
    {
    case 1:
        HPF(readyQueue);
        break;
    case 2:
        /* SRTN */
        SRTN();
        break;
    case 3:
        RR(readyQueue, timeQuantum);
        break;

    default:
        break;
    }
    fclose(logFile);
    createPerfFile();
    // upon termination release the clock resources.

    destroyClk(true);
}

// no longer 3arf eh lazmet el function de
void switchProcess(Process **runningProcess, Process *p)
{
    if (p == NULL)
        return;
    if (*runningProcess)
        (*runningProcess)->state = 1;
    *runningProcess = p;
    (*runningProcess)->state = 2;
}



void SRTN(){

}


void HPF(PriorityQueuePointer readyQueue)
{
    while (!priority_queue_empty(readyQueue))
    {
        while (time == getClk())
        {
        };
        time = getClk();
        if (!(runningProcess))
        {
            switchProcess((&runningProcess), priority_queue_remove(readyQueue));
            int pid = fork();
            if (pid == -1)
                perror("Error in forking a process!");
            else if (pid == 0)
            {
                char arg1[10];
                sprintf(arg1, "%d", (runningProcess)->remainingTime);
                execl("./process.out", "process", arg1, NULL);
            }
            else
            {
                int statlock;
                int cid = wait(&statlock); // to ensure that it waits for the running process to finish
                if (WIFEXITED(statlock))
                    printf("A process terminated successfully with status %d.", WEXITSTATUS(statlock));
                else
                    printf("Some thing went wrong with the process.");
            }
        }
    }
}

void RR(PriorityQueuePointer readyQueue, int timeQuantum)
{
    while (!priority_queue_empty(readyQueue))
    {
        runningProcess = priority_queue_remove(readyQueue);
        if (runningProcess->state == 0)
        {
            int pid = fork();
            if (pid == -1)
                perror("Error in forking a process!");
            else if (pid == 0)
            {
                char arg1[10];
                sprintf(arg1, "%d", runningProcess->remainingTime);
                execl("./process.out", "process", arg1, NULL);
            }
            else
            {
                runningProcess->pid = pid;
            }
        }
        writeToLogFile(0);
        runningProcess->state = 2;
        Process *p = runningProcess;
        p->waitingTime += time - p->lastStoppedTime;
        int remainingTime = timeQuantum;
        kill(p->pid, SIGUSR1);
        while (remainingTime > 0)
        {
            while (time == getClk())
                ;
            time = getClk();
            p->remainingTime--;
            if (p->remainingTime == 0)
            {
                p->state = 3;
                break;
            }
            remainingTime--;
        }
        kill(p->pid, SIGUSR2);
        if (p->remainingTime > 0)
        {
            insert_into_tail(p, readyQueue);
            p->lastStoppedTime = time;
            p->state = 1;
            writeToLogFile(1);
        }
    }
}
// 1. generator sends process to scheduler
// 2. scheduler fork the process and store its pid
// 3. scheduling algorithm let the process run and decrement its remaining time

void receiveProcess(int signum)
{
    Process *p = malloc(sizeof(Process));
    msgbuff message;
    int recval = msgrcv(msgq_id, &message, sizeof(message.msg_process), 1, !IPC_NOWAIT);
    *p = message.msg_process;
    if (recval == -1)
    {
        perror("Error in receiving from generator");
    }
    else
    {
        printf("recieved with arrival time : %d", p->arrivalTime);
        totalProcesses++;
        totalExecutionTime += p->runTime;
        p->state = 0;
        p->lastStoppedTime = time;
        PCBTable[p->id] = p;
        switch (algorithm)
        {
        case 1:
            priority_queue_insert(p, readyQueue, 1);
            break;
        case 2:
            priority_queue_insert(p, readyQueue, 0);
            break;
        case 3:
            insert_into_tail(p, readyQueue);
            break;
        }
    }
}

void writeToLogFile(int state)
{
    if (logFile == NULL)
    {
        perror("Error in opening the log file");
        exit(-1);
    }
    switch (state)
    {
    case 0:
        if ((runningProcess)->state == 0)
            fprintf(logFile, "At time %d process %d started arr %d total %d remain %d wait %d\n", time, (runningProcess)->id, runningProcess->arrivalTime, runningProcess->runTime, runningProcess->remainingTime, runningProcess->waitingTime);
        else
            fprintf(logFile, "At time %d process %d resumed arr %d total %d remain %d wait %d\n", time, runningProcess->id, runningProcess->arrivalTime, runningProcess->runTime, runningProcess->remainingTime, runningProcess->waitingTime);
        break;
    case 1:
        fprintf(logFile, "At time %d process %d stopped arr %d total %d remain %d wait %d\n", time, runningProcess->id, runningProcess->arrivalTime, runningProcess->runTime, runningProcess->remainingTime, runningProcess->waitingTime);
        break;
    case 2:
        fprintf(logFile, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %f\n", time, runningProcess->id, runningProcess->arrivalTime, runningProcess->runTime, runningProcess->remainingTime, runningProcess->waitingTime, time - runningProcess->arrivalTime, (float)(time - runningProcess->arrivalTime) / runningProcess->runTime);
        totalWTA += (float)(time - (runningProcess)->arrivalTime) / (runningProcess)->runTime;
        totalWTA2 += pow((float)(time - (runningProcess)->arrivalTime) / (runningProcess)->runTime, 2);
        totalWaitingTime += (runningProcess)->waitingTime;
        break;
    }
}

void processFinishedHandler(int signum)
{
    writeToLogFile(2);
    deleteProcess();
}

void createPerfFile()
{
    FILE *perfFile = fopen("scheduler.perf", "w");
    if (perfFile == NULL)
    {
        perror("Error in opening the performance file");
        exit(-1);
    }
    fprintf(perfFile, "CPU utilization = %f\n", (float)totalExecutionTime / time);
    fprintf(perfFile, "Avg WTA = %f\n", totalWTA / totalProcesses);
    fprintf(perfFile, "Std WTA = %f\n", sqrt((totalWTA2 / totalProcesses) - pow(totalWTA / totalProcesses, 2)));
    fprintf(perfFile, "Avg Waiting = %f\n", (float)totalWaitingTime / totalProcesses);
    fclose(perfFile);
}

void deleteProcess()
{
    free(PCBTable[runningProcess->id]);
    runningProcess = NULL;
}