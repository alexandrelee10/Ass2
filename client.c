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

// COMPILE: gcc -o client client.c -lpthread -lrt
// RUN: ./client
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <signal.h>

#define MAX_CMD_LEN 256
#define MSG_QUEUE_KEY 12345

// Message structure for the message queue
struct msg_buffer {
    long msg_type;
    char msg_text[MAX_CMD_LEN];
};

// Function to handle server shutdown messages
void *monitor_shutdown(void *arg) {
    int msgid = *((int *)arg);
    struct msg_buffer message;
    while (1) {
        // Receive shutdown message
        if (msgrcv(msgid, &message, sizeof(message), 2, 0) != -1) {
            if (strcmp(message.msg_text, "SHUTDOWN") == 0) {
                printf("Server is shutting down...\n");
                exit(0);  // Terminate the client
            }
        }
    }
    return NULL;
}

// Function to send commands to the server
void send_command(int msgid, const char *command) {
    struct msg_buffer message;
    message.msg_type = 1;
    // Include client PID
    snprintf(message.msg_text, MAX_CMD_LEN, "%d %s", getpid(), command);
    if (msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
        perror("msgsnd failed");
        exit(1);
    }
    printf("Sent command: %s\n", message.msg_text);
}

// Function to handle user input commands
void handle_user_input(int msgid, char *command) {
    // Check for empty input or commands with only spaces
    if (strlen(command) == 0 || strspn(command, " ") == strlen(command) || strspn(command, "") == strlen(command)) {
        printf("Invalid input. Please enter a valid command.\n");
        return;
    }
    // Handle valid commands
    if (strncmp(command, "CHPT", 4) == 0) {
        // Extract the new prompt
        char new_prompt[MAX_CMD_LEN];
        if (sscanf(command, "CHPT %[^\n]", new_prompt) == 1) {
            printf("Prompt changed to: %s\n", new_prompt);
        } else {
            printf("Invalid CHPT command. Usage: CHPT <new_prompt>\n");
        }
    }
    if (strcmp(command, "EXIT") == 0) {
        send_command(msgid, "EXIT");
        printf("Client disconnecting...\n");
    } else if (strcmp(command, "LIST") == 0) {
        send_command(msgid, "LIST");
    } else if (strcmp(command, "HIDE") == 0) {
        send_command(msgid, "HIDE");
    } else if (strcmp(command, "UNHIDE") == 0) {
        send_command(msgid, "UNHIDE");
    } else if (strcmp(command, "exit") == 0) {
        printf("Ignored 'exit' command as it may exit the shell session...\n");
    } else if (strcmp(command, "chpt new_prompt") == 0) {
        send_command(msgid, "chpt new_prompt");
    } else if (strcmp(command, "CHPT") == 0) {
        send_command(msgid, "CHPT");
    } else if (strcmp(command, "EXIT NOW") == 0) {
        send_command(msgid, "EXIT NOW");
    } else if (strcmp(command, "LIST all") == 0) {
        send_command(msgid, "LIST all");
    } else if (strcmp(command, "HIDE client") == 0) {
        send_command(msgid, "HIDE client");
    } else if (strcmp(command, "UNHIDE user") == 0) {
        send_command(msgid, "UNHIDE user");
    }else if (strcmp(command, "SHUTDOWN") == 0) {
        printf ("Invalid because SHUTDOWN is a server-initiated broadcast command and cannot be sent by the client.");
    } else {
        send_command(msgid, command);  // Send the command to the server
    }
}

// Main function
int main() {
    // Initialize message queue
    int msgid = msgget(MSG_QUEUE_KEY, 0666);
    if (msgid == -1) {
        perror("msgget failed");
        exit(1);
    }

    // Create a child thread to monitor for SHUTDOWN messages from the server
    pthread_t shutdown_thread;
    pthread_create(&shutdown_thread, NULL, monitor_shutdown, &msgid);
    pthread_detach(shutdown_thread);

    while (1) {
        char command[MAX_CMD_LEN];
        printf("Enter command: ");
        fgets(command, MAX_CMD_LEN, stdin);

        // Remove newline character from the input
        command[strcspn(command, "\n")] = '\0';

        // Handle user-defined commands
        handle_user_input(msgid, command);

        if (strcmp(command, "EXIT") == 0) {
            break;  // Exit the client gracefully
        }
    }
    return 0;
}
