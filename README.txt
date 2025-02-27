The CODE directory will have the following:
===========================================

*.c -> All .c source files.

*.h -> Any optional header files.

Makefile -> With commands: make and make clean. make should produce both 'server' and 'client' executables separately.

Executable -> Statically linked executables of your Client and Server implementations. Try running it on multiple computers to check its portability.



The README.txt file must include:
================================
- Each Team Member Information( Name, PID, FIU email, section <RVC, RVD, RVE, U01, U02> )
(RVC) Kevin Tran, 6441265, ktran030@fiu.edu
(RVC) Alexandre Lee, 6425788, alee136@fiu.edu
(RVC) Kevin Calvoo, 6451725, kcalv015@fiu.edu
(RVC) Tortrong Yang, 6447079, tyang019@fiu.edu 

- Any specific guidelines, notes, or considerations about the project compilation and execution.

- Brief explanation about how your selected IPC mechanism(s) align with the design and requirements of this assignment, and why they are the most appropriate choice for your implementation.

A message queue was implemented to enable asynchronous communication between the server and the clients. 
The message queue allows the server to receive a command by the client without blocking, enabling the server
handle multiple commands concurrently. The server can receive messages from queue and process each client's 
commands independently. The client is automatically registered and can enter a command, such as, HIDE, LIST, or UNHIDE,
and the server will pick up the command, process the message in the message queue, and display a message in the terminal.

*** Please refer to the course syllabus for additional assignment submission requirements and guidelines.

COMPILE server: gcc -o server server.c -lpthread (ONLY ADD IF ONE WINDOWS)-lrt 
RUN server: ./server

COMPILE client: gcc -o client client.c -lpthread -lrt
RUN client: ./client

COMPILE executable file in client: gcc -o executable server.c
RUN file: ./executable