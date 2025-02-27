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
int msgid;
// Message structure for the message queue
struct msg_buffer {
    long msg_type;
    char msg_text[MAX_CMD_LEN];
};
// Structure to pass both client_pid and command to the thread
struct command_args {
    int client_pid;
    char command[MAX_CMD_LEN];
};

pthread_t thread_id;

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
void handle_user_input(int msgid, char *command);

pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_hidden[MAX_CLIENTS] = {0};  // Tracks hidden clients
int client_count = 0;  // Tracks the number of connected clients
int client_list[MAX_CLIENTS] = {0};

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    printf("\nServer shutting down...\n");

    pthread_mutex_lock(&client_list_mutex);

    // Send SHUTDOWN message to all clients
    struct msg_buffer shutdown_msg;
    shutdown_msg.msg_type = 1;  // Using 1 as broadcast type

    strncpy(shutdown_msg.msg_text, "SHUTDOWN", MAX_CMD_LEN);

    for (int i = 0; i < client_count; i++) {
        shutdown_msg.msg_type = client_list[i];  // Send to each client individually
        if (msgsnd(msgid, &shutdown_msg, sizeof(shutdown_msg) - sizeof(long), 0) == -1) {
            perror("Failed to send shutdown message");
        }
    }

    pthread_mutex_unlock(&client_list_mutex);

    // Cleanup resources
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl (IPC_RMID) failed");
    }

    printf("All resources freed. Exiting...\n");
    exit(0);
}

void *execute_command(void *arg) {
    if (!arg) {
        fprintf(stderr, "Error: NULL argument received in execute_command.\n");
        return NULL;
    }

    struct command_args *args = (struct command_args *)arg;
    char *command = args->command;
    int client_pid = args->client_pid;

    // Trim leading spaces
    while (*command == ' ') command++;

    // Ensure the command is not empty
    if (command[0] == '\0') {  // Proper check for empty command
        handle_invalid_command(command, "Invalid: Empty command received.");
        free(arg);
        return NULL;
    }

    printf("Executing command: '%s' (Client PID: %d)\n", command, client_pid);

    // Handle CHPT command with proper input validation
    if (strncmp(command, "CHPT", 4) == 0) {
        char *arg_start = command + 4;

        // Trim spaces after "CHPT"
        while (*arg_start == ' ') arg_start++;

        if (*arg_start == '\0') {  // If no argument follows
            handle_invalid_command(command, "<new_prompt>"); // Display error message
        } else {
            printf("CHPT command received with argument: '%s'\n", arg_start);
            // Handle the CHPT command normally
        }
    } 
    else if (strcmp(command, "shutdown") == 0) {
        printf("Shutdown command received. Terminating server.\n");
        free(arg);
        exit(0);
    } 
    else if (strcmp(command, "status") == 0) {
        printf("Server is running normally.\n");
    } 
    else if (strcmp(command, "HIDE") == 0) {
        handle_hide(client_pid);  // Call the handle_hide function
    } 
    else if (strcmp(command, "UNHIDE") == 0) {
        handle_unhide(client_pid);  // Call the handle_unhide function
    }
    else if (strcmp(command, "LIST") == 0) {
        handle_list();  // Call the handle_list function
    }
    else if (strcmp(command, "EXIT") == 0) {
        handle_exit(client_pid);
    }
    else {
        printf("Unknown command: '%s'\n", command);
    }

    free(arg);
    return NULL;
}
// Function to handle commands in the message queue
void handle_commands(int msgid) {
    struct msg_buffer message;

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

        // Declare thread_id **before** using it
        pthread_t thread_id;

        // Allocate memory for arguments
        struct command_args *args = malloc(sizeof(struct command_args));
        if (!args) {
            perror("malloc failed");
            continue;
        }

        args->client_pid = client_pid;
        strncpy(args->command, command, MAX_CMD_LEN);
        args->command[MAX_CMD_LEN - 1] = '\0';  // Ensure null termination

        // Create a new thread to execute the command
        if (pthread_create(&thread_id, NULL, execute_command, args) != 0) {
            perror("pthread_create failed");
            free(args);  // Free memory if thread creation fails
        }

        // Detach the thread so it cleans up automatically
        pthread_detach(thread_id);
    }
}

// Function to handle invalid commands
void handle_invalid_command(char *cmd, char *msg) {
    printf("Error: %s (Command: '%s')\n", msg, cmd);
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

    // Extract the new prompt argument from the CHPT command
    // CHPT new_prompt_value
    if (sscanf(cmd, "CHPT %[^\n]", new_prompt) == 1) {
        // Print the message with the new prompt
        printf("Client changed prompt to: %s\n", new_prompt);
    } else {
        printf("Invalid command format for 'CHPT'. Ensure the new prompt is provided.\n");
    }
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
        printf("Client %d Disconnected.\n", client_pid);  // Message displayed in terminal

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
                printf("Client %d: You Are Already Hidden.\n", client_pid);
            } else {
                client_hidden[i] = 1;
                printf("Client %d: You Are Now hidden.\n", client_pid);
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
                printf("Client %d: You Are Not Hidden.\n", client_pid);
            } else {
                client_hidden[i] = 0;
                printf("Client %d: You Are Now Visible Again.\n", client_pid);
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

void handle_shutdown() {
    printf("Server shutting down...\n");
    exit(0);
}

void execute_in_shell(char *cmd) {
    // Check for commands where the argument should be separated by a space
    if (strncmp(cmd, "ls-l", 4) == 0) {
        handle_invalid_command(cmd, "Invalid: 'ls-l' should be 'ls -l'. Missing space between command and flag.");
        return;
    }
    if (strncmp(cmd, "echo", 4) == 0) {
        // Ensure that 'echo' is followed by a space and some content
        if (strlen(cmd) == 4) {
            handle_invalid_command(cmd, "Invalid: 'echo' requires a space and text to be printed.");
            return;
        }
        // Check if there is no space after echo, handle it as invalid
        if (cmd[4] != ' ') {
            handle_invalid_command(cmd, "Invalid: 'echo' requires a space between 'echo' and the text.");
            return;
        }
    }    
    if (strncmp(cmd, "cat", 3) == 0) {
        // Ensure that 'cat' is followed by a space and a file name
        if (strlen(cmd) == 3) {
            handle_invalid_command(cmd, "Invalid: 'cat' requires a file name.");
            return;
        }
        // Check if there is no space after 'cat', handle it as invalid
        if (cmd[3] != ' ') {
            handle_invalid_command(cmd, "Invalid: 'cat' requires a space between 'cat' and the file name.");
            return;
        }
    }
    if (strncmp(cmd, "./", 2) == 0) {
        // Extract the binary name
        char binary[MAX_CMD_LEN];
        sscanf(cmd, "./%s", binary);

        // Check if the file exists and is executable
        if (access(binary, X_OK) == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                execl(cmd, cmd, (char *)NULL);  // Execute the binary
                perror("execl failed");
                exit(1);
            } else {
                sleep(TIMEOUT);
                if (waitpid(pid, NULL, WNOHANG) == 0) {
                    kill(pid, SIGKILL);
                    printf("Command Timeout: Killing process %d\n", pid);
                }
            }
        } else {
            handle_invalid_command(cmd, "Error: File does not exist or is not executable.");
        }
        return;
    }    
    if (strncmp(cmd, "mkdir", 5) == 0) {
        // Ensure that 'mkdir' is followed by a space and a folder name
        if (strlen(cmd) == 5) {
            handle_invalid_command(cmd, "Invalid: 'mkdir' requires a folder name.");
            return;
        }
        // Check if there is no space after mkdir, handle it as invalid
        if (cmd[5] != ' ') {
            handle_invalid_command(cmd, "Invalid: 'mkdir' requires a space between 'mkdir' and the folder name.");
            return;
        }
    }    

    if (strncmp(cmd, "grep patternfile.txt", 21) == 0) {
        handle_invalid_command(cmd, "Invalid: 'grep patternfile.txt' should be 'grep pattern file.txt'. Missing space.");
        return;
    }

    // Check for other invalid cases where commands should have arguments
    if (strncmp(cmd, "rm", 2) == 0 && strlen(cmd) == 2) {
        handle_invalid_command(cmd, "Invalid: 'rm' requires a file or directory to delete.");
        return;
    }
    // If no invalid case detected, execute the command
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


// To Do:
// 1. Empty Input
// 2. " " Fix Cases
// 3. 