/* This assignment focuses on developing a multi-threaded client-server application in C that
handles asynchronous (non-blocking) communication between multiple clients and a server. The
server acts as a Linux shell that processes various commands sent by clients and responds to the
clients appropriately. The assignment aims to deepen the understanding of multithreading,
process management, inter-process communication, and graceful termination of programs.

A message queue was implemented to enable asynchronous communication between the server and the clients. 
The message queue allows the server to receive a command by the client without blocking, enabling the server
handle multiple commands concurrently. The server can receive messages from queue and process each client's 
commands independently. 
*/

// COMPILE: gcc -o server server.c -lpthread (ONLY ADD IF ONE WINDOWS)-lrt 
// RUN: ./server

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_CMD_LEN 256
#define MSG_QUEUE_KEY 12345
#define MAX_CLIENTS 3
#define TIMEOUT 3

// Message structure for the message queue
struct msg_buffer {
    long msg_type;
    char msg_text[MAX_CMD_LEN];
};

// Function declarations (prototypes)
void handle_signal(int sig);
void handle_commands(int msgid);
void *execute_command(void *arg);
void handle_chpt(char *cmd);
void handle_exit(int client_pid);
void handle_list();
void handle_hide(int client_pid);
void handle_unhide(int client_pid);
void handle_exit_command();
void handle_invalid_command(char *cmd, char *msg);
void handle_shutdown();
void execute_in_shell(char *cmd);
void register_client(int client_pid);

pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_hidden[MAX_CLIENTS] = {0};  // Tracks hidden clients
int client_count = 0;  // Tracks the number of connected clients
int client_list[MAX_CLIENTS] = {0};

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    printf("Server shutting down...\n");
    exit(0);
}

// Function to handle commands in the message queue
void handle_commands(int msgid) {
    struct msg_buffer message;
    struct msg_buffer response;
    while (1) {
        // Receive a message from the client
        if (msgrcv(msgid, &message, sizeof(message) - sizeof(long), 1, 0) == -1) {
            perror("msgrcv failed");
            continue;
        }
        printf("Received raw message: %s\n", message.msg_text);
        int client_pid;
        char command[MAX_CMD_LEN];

        // Extract the client PID first
        char *space_pos = strchr(message.msg_text, ' ');  // Find first space
        if (space_pos == NULL) {
            printf("Invalid message format: No command found.\n");
            continue;
        }
        *space_pos = '\0';  // Split the string
        client_pid = atoi(message.msg_text);  // Convert PID from string to int
        strncpy(command, space_pos + 1, MAX_CMD_LEN);
        command[MAX_CMD_LEN - 1] = '\0';  // Ensure null termination
        printf("Client PID: %d | Command: %s\n", client_pid, command);

        // Register the client before processing the command
        register_client(client_pid);

        // Execute the command in a separate thread
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, execute_command, strdup(command));
        pthread_detach(thread_id);
    }
}

// Function to handle command execution
void *execute_command(void *arg) {
    char *command = (char *)arg;  // Command string only
    printf("Executing command: %s\n", command);
    // Process commands
    if (strncmp(command, "CHPT", 4) == 0) {
        handle_chpt(command);
    } else if (strncmp(command, "EXIT", 4) == 0) {
        printf("EXIT received. Ignoring in execute_command().\n");
    } else if (strcmp(command, "LIST") == 0) {
        handle_list();
    } else if (strcmp(command, "HIDE") == 0) {
        printf("HIDE received. Ignoring in execute_command().\n");
    } else if (strcmp(command, "UNHIDE") == 0) {
        printf("UNHIDE received. Ignoring in execute_command().\n");
    } else if (strcmp(command, "exit") == 0) {
        handle_exit_command();
    } else if (strcmp(command, "SHUTDOWN") == 0) {
        handle_shutdown();
    } else {
        execute_in_shell(command);
    }
    free(command);  // Free dynamically allocated command string
    return NULL;
}

// Register Clients
void register_client(int client_pid) {
    pthread_mutex_lock(&client_list_mutex);
    // Check if the client is already registered
    for (int i = 0; i < client_count; i++) {
        if (client_list[i] == client_pid) {
            pthread_mutex_unlock(&client_list_mutex);
            return; // Client is already registered
        }
    }
    // Register new clients if space is available
    if (client_count >= MAX_CLIENTS) {
        printf("Max clients reached. Cannot register client %d\n", client_pid);
        pthread_mutex_unlock(&client_list_mutex);
        return;
    }
    // Store client PID
    client_list[client_count] = client_pid;
    client_hidden[client_count] = 0;
    client_count++;
    
    printf("Client %d registered\n", client_pid);
    pthread_mutex_unlock(&client_list_mutex);
}

// Command handlers
void handle_chpt(char *cmd) {
    char new_prompt[MAX_CMD_LEN];
    sscanf(cmd, "CHPT %s", new_prompt);
    printf("Client changed prompt to: %s\n", new_prompt);
}

void handle_exit(int client_pid) {
    pthread_mutex_lock(&client_list_mutex);
    
    int found = -1;  // Track client index
    for (int i = 0; i < client_count; i++) {
        if (client_list[i] == client_pid) {
            found = i;
            break;
        }
    }

    if (found != -1) {
        printf("Client %d disconnected.\n", client_pid);

        // Shift array left to remove the client
        for (int j = found; j < client_count - 1; j++) {
            client_list[j] = client_list[j + 1];
            client_hidden[j] = client_hidden[j + 1];  // Shift hidden status too
        }
        client_count--;  // Reduce count
    } else {
        printf("Client %d not found.\n", client_pid);
    }

    pthread_mutex_unlock(&client_list_mutex);
}
// Handle the list 
void handle_list() {
    pthread_mutex_lock(&client_list_mutex);
    if (client_count == 0) {
        printf("No clients connected.\n");
        pthread_mutex_unlock(&client_list_mutex);
        return;
    }
    printf("Connected Clients: ");
    for (int i = 0; i < client_count; i++) {
        if (!client_hidden[i]) {
            printf("%d ", client_list[i]);  // Print actual client PID
        }
    }
    printf("\n");
    pthread_mutex_unlock(&client_list_mutex);
}


void handle_hide(int client_pid) {
    pthread_mutex_lock(&client_list_mutex);

    for (int i = 0; i < client_count; i++) {
        if (client_list[i] == client_pid) {
            if (client_hidden[i]) {
                printf("Client %d is already hidden.\n", client_pid);
            } else {
                client_hidden[i] = 1;
                printf("Client %d is now hidden.\n", client_pid);
            }
            pthread_mutex_unlock(&client_list_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&client_list_mutex);
}

void handle_unhide(int client_pid) {
    pthread_mutex_lock(&client_list_mutex);

    for (int i = 0; i < client_count; i++) {
        if (client_list[i] == client_pid) {
            if (!client_hidden[i]) {
                printf("Client %d is not hidden.\n", client_pid);
            } else {
                client_hidden[i] = 0;
                printf("Client %d is now visible again.\n", client_pid);
            }
            pthread_mutex_unlock(&client_list_mutex);
            return;
        }
    }

    pthread_mutex_unlock(&client_list_mutex);
}

void handle_exit_command() {
    printf("Ignored 'exit' command as it may exit the shell session...\n");
}

void handle_invalid_command(char *cmd, char *msg) {
    printf("Command '%s': %s\n", cmd, msg);
}

void handle_shutdown() {
    printf("Server shutting down...\n");
    exit(0);
}

void execute_in_shell(char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("/bin/bash", "bash", "-c", cmd, (char *)NULL);
        perror("execlp failed");
        exit(1);
    } else {
        // Monitor command execution time (timeout after 3 seconds)
        sleep(TIMEOUT);
        if (waitpid(pid, NULL, WNOHANG) == 0) {
            kill(pid, SIGKILL);  // Kill process if it exceeds 3 seconds
            printf("Command Timeout: Killing process %d\n", pid);
        }
    }
}

// Main starts here
int main() {
    signal(SIGINT, handle_signal);  // Handle Ctrl+C gracefully

    // Initialize message queue
    int msgid = msgget(MSG_QUEUE_KEY, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget failed");
        exit(1);
    }
    printf("Server started. Waiting for client commands...\n");
    handle_commands(msgid);  // Start handling commands from the message queue
    return 0;
}



// 1. WORK ON SHUTDOWN BECAUSE WHEN SENT BY CLIENT IT SHOULD NOT SHUT DOWN
// 2. WORK ON HIDE COMMAND AND MAKE SURE IT DOESNT APPEAR IN LIST COMMAND 
// 3. WORK ON EMPTY INPUT EDGE CASES
// 4. ENSURE CLIENT CANNOT SHUTDOWN SERVER SERVER SHOULD CLOSE THROUGH CTRL + C