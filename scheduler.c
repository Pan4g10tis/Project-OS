#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_FILENAME_LENGTH 256

enum ProcessStatus {
    NEW,
    RUNNING,
    STOPPED,
    EXITED
};

struct Application {
    char filename[MAX_FILENAME_LENGTH];
    pid_t pid;
    enum ProcessStatus status;
    double executionTime;
};

struct ExecutionQueueNode {
    struct Application app;
    struct ExecutionQueueNode* next;
    struct ExecutionQueueNode* prev;
};

struct ExecutionQueue {
    struct ExecutionQueueNode* front;
    struct ExecutionQueueNode* rear;
};

// Function declarations
void enqueueApplication(struct ExecutionQueue* queue, const char* filename, enum ProcessStatus status, double time);
void dequeueApplication(struct ExecutionQueue* queue);
void runFCFSScheduler(struct ExecutionQueue* queue);
void runRoundRobinScheduler(struct ExecutionQueue* queue, int quantum);
void sigchldHandler(int signo);

// Global variable to track the currently running process
struct ExecutionQueueNode* currentProcess = NULL;

// Global variable for Execution Queue
struct ExecutionQueue queue;

// Global time variables
clock_t start, finish; 

void enqueueApplication(struct ExecutionQueue* queue, const char* filename, enum ProcessStatus status, double time) {
    // Create a new ExecutionQueueNode
    struct ExecutionQueueNode* newNode = (struct ExecutionQueueNode*)malloc(sizeof(struct ExecutionQueueNode));
    if (newNode == NULL) {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }
    
    // Initialize the Application
    strcpy(newNode->app.filename, filename);
    newNode->app.status = status;
    newNode->app.executionTime = time;

    // Set up the linked list pointers
    newNode->next = NULL;
    newNode->prev = queue->rear;

    // Update the rear pointer of the queue
    if (queue->rear == NULL) {
        // If the queue is empty, set front pointer as well
        queue->front = newNode;
    } else {
        queue->rear->next = newNode;
    }
    queue->rear = newNode;
}

void dequeueApplication(struct ExecutionQueue* queue) {
    if (queue->front == NULL) {
        fprintf(stderr, "Queue is empty\n");
        exit(EXIT_FAILURE);
    }
    
    struct ExecutionQueueNode* temp = queue->front;
    
    // Update the front pointer
    queue->front = temp->next;


    // Update the current process pointer
    currentProcess = queue->front;
    
    // Free the memory of the dequeued node
    free(temp);

    // If the queue becomes empty, update the rear pointer as well
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
}

void runFCFSScheduler(struct ExecutionQueue *queue) {

    start = clock();

    while (queue->front != NULL) {
    
        
        // Fork a new process
        pid_t pid = fork();
        if (pid == -1) {
              perror("fork");
              exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            // Execute the application
            execlp(currentProcess->app.filename, currentProcess->app.filename, (char *)NULL);
            perror("execlp");
            exit(EXIT_FAILURE);
        } else {
            // Wait for the process to finish
            waitpid(pid, NULL, 0);
            
            // Calculate time
            finish = clock();
            currentProcess->app.executionTime = ((double)(finish - start)); 
            
            // Print information
            printf("Process %s completed. Execution time: %f\n",
                  currentProcess->app.filename, currentProcess->app.executionTime);
                  
            // Dequeue the next application
            dequeueApplication(queue);
        }
    }
}

void runRoundRobinScheduler(struct ExecutionQueue* queue, int quantum) {

    clock_t start, finish;
    start = clock();

    while (queue->front != NULL) {

        // Fork a new process
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            // Execute the application
            execlp(currentProcess->app.filename, currentProcess->app.filename, (char*)NULL);
            perror("execlp");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            currentProcess->app.pid = pid;
            currentProcess->app.status = RUNNING;

            // Wait for the time quantum or until the process finishes
            sleep(quantum);

            // Send SIGSTOP to suspend the process
            kill(pid, SIGSTOP);

            // Update execution time
            finish = clock();
            currentProcess->app.executionTime += ((double)(finish - start));

            // Move the process to the end of the queue
            enqueueApplication(queue, currentProcess->app.filename, STOPPED, currentProcess->app.executionTime);

            // Send SIGCONT to resume the process
            kill(pid, SIGCONT);

            // Wait for the process to finish
            waitpid(pid, NULL, 0);

            // Update the process status
            currentProcess->app.status = EXITED;
            
            // Dequeue the next application
            dequeueApplication(queue);
        }
    }
}

void sigchldHandler(int signo) {
    // Handler for SIGCHLD signal
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Find the corresponding process in the queue
        struct ExecutionQueueNode* temp = queue.front;
       
        while (temp != NULL && temp->app.pid != pid) {
            temp = temp->next;
        }
       
        if(temp != NULL){
            // Update the process status
            temp->app.status = EXITED;
            
            // Print information
            finish = clock();
            currentProcess->app.executionTime += ((double)(finish - start));
            printf("Process %s completed. Execution time: %f\n",
                  temp->app.filename, temp->app.executionTime);

            // Dequeue the process from the queue
            dequeueApplication(&queue);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3 && argc !=4) {
        fprintf(stderr, "Usage: %s <quantum> <input_filename1>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* algorithm = argv[1];
    char* inputFilename;
    int quantum;
    
    if (strcmp(algorithm, "FCFS") == 0){
        inputFilename = argv[2];
    } else if (strcmp(algorithm, "RR") == 0){
        quantum = atoi(argv[2]);
        inputFilename = argv[3];
    }

    // Set up signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = sigchldHandler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    // Initialize the execution queue
    queue.front = NULL;
    queue.rear = NULL;

   
    FILE* file = fopen(inputFilename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_FILENAME_LENGTH];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character from the end of the line
        line[strcspn(line, "\n")] = '\0';
        enqueueApplication(&queue, line, NEW, 0);
    }
    fclose(file);
    
    currentProcess = queue.front;

    // Run the scheduler based on the specified algorithm
    if (strcmp(algorithm, "FCFS") == 0){
        runFCFSScheduler(&queue); 
    }else if (strcmp(algorithm, "RR") == 0){
        runRoundRobinScheduler(&queue, quantum);
    }

    return 0;
}
