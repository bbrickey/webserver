/**
 * @file main.cpp
 * @author Ben Brickey
 * @brief Working Web Server - handles client request for existing files, uses virtual memory to map
 * contents of shakespeare, displays environment variables, returns file not found error, program
 * ends upon "quit" request
 * browser link: http://localhost:8080/index.html
 * @version 1.0
 * @date 2023-03-14
 *
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include <vector>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "httpcontext.h"

using namespace std;

#define BUF_SIZE 1025
#define PORT 8080
#define TRUE   1
#define MAX_CLIENTS 30

extern char** environ;
int pipefds[2];
pthread_mutex_t lock1;
pthread_attr_t loggerAttr;
bool memoryMapped;
char *mapped;

long get_tid_xplat();

void *runLogger(void *param);

void runServer();

void *childThread(void* param);

void handleFile(http_context);

void handleShakespeare(http_context, bool);

void quitProgram(string, int);

void fileNotFound(string, int);

void getEnvironment(int, string);

vector<string> split(string&, char);

string getHeader(int, string);

string getLogger(string, string, int);


int main() {

    memoryMapped = true; // ensures mmap function runs once

    pipe(pipefds); //creates pipe and stores file descriptors

    //LOGGER PTHREAD
    pthread_t logger;
    pthread_attr_init(&loggerAttr);
    pthread_create(&logger, &loggerAttr, &runLogger, NULL); // being logger thread to log status to screen

    runServer(); // begin server thread to enable client requests from browser

    pthread_join(logger, NULL);
    return 0;
}

//Logger prints client requests to screen
void *runLogger(void *param) {
    printf("Logger pid: %d tid: %ld\n", getpid(), get_tid_xplat());
    printf("Listening on Port: %d \n", PORT);

    while (true) {
        char buffer[BUF_SIZE]; // buffer to read file descriptors
        ssize_t bytes_read = read(pipefds[0], buffer, sizeof(buffer));
        for (int i = 0; i<bytes_read; i++) {
            cout << buffer[i];
        }
    }
    pthread_exit(0);
}

//socket programming code from https://www.codingninjas.com/codestudio/library/learning-socket-programming-in-c
void runServer() {

    pthread_mutex_init(&lock1, NULL);
    int master_sock, addrlen, new_sock, client_sock[MAX_CLIENTS], sock_descriptor, act;
    int maximum_socket_descriptor;
    struct sockaddr_in adr{};
    fd_set readfds; //set of file descriptors

    //Create a socket with the socket() system call
    if ((master_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) //creating a master socket
    {
        perror("Failed_Socket");
        exit(EXIT_FAILURE);
    }

    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = INADDR_ANY;
    adr.sin_port = htons(PORT);

    //Bind the socket to an address using the bind() system call.
    if (::bind(master_sock, (struct sockaddr *) &adr, sizeof(adr)) < 0)
    {
        perror("Failed_Bind");
        exit(EXIT_FAILURE);
    }

    //Listen for connections with the listen() system call
    if (listen(master_sock, 5) < 0) //Specify 3 as maximum pending connections for master socket
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //Accept a connection with the accept() system call..
    addrlen = sizeof(adr); //Accepting the Incoming Connection

    while(TRUE) {
        FD_ZERO(&readfds); // clearing the socket set
        FD_SET(master_sock, &readfds); //adding master socket to set

        int nextIndex = 0;

            int new_sock;
            if ((new_sock = accept(master_sock,
                                   (struct sockaddr *) &adr, (socklen_t *) &addrlen)) < 0) {
                perror("Accept!");
                exit(EXIT_FAILURE);
            }

            //create child threads
            pthread_t child;
            if (pthread_create(&child, NULL, &childThread, &new_sock) != 0) {
                perror("Failed to create thread");
                exit(EXIT_FAILURE);
            }


            if (pthread_join(child, nullptr) != 0) {
                perror("Child thread did not finish");
                exit(EXIT_FAILURE);
            }

    }
}

//child thread created for each client request from browser
void *childThread (void * param) {
    size_t value_read;
    char buffer[BUF_SIZE];
    int socketNum = *(int*)param;
    int msgsize = 0;

    //read message from client
    if ((value_read = read(socketNum, buffer, 255)) == 0) {
        perror("ERROR reading from socket");
        exit(EXIT_FAILURE);
    }

    //STRING MANIPULATION FOR RESPONSE
    string msg = string(buffer);
    //GET REQUESTED FILE
    vector<string> getRequest = split(msg, '/');
    vector<string> getRequest2 = split(getRequest[1],' ');
    //check if request is for partial or full
    string requestedFile;
    bool partialFile = false;
    partialFile = getRequest2[0].find("?") != std::string::npos;
    if (partialFile) {
        requestedFile = "shakespeare.txt";
    } else {
        requestedFile = getRequest2[0];
    }

    //GET FILE TYPE (png, html, etc)
    vector<string> getType = split(msg,'.');
    vector<string> type = split(getType[1], ' ');

    //STRING MANIPULATION FOR LOGGER
    //get client request line ("GET / fish.png", etc)
    vector<string> loggerRequest = split(msg, 'H');
    string logRequestStr = loggerRequest[0];
    string loggerOutput;

    //open requested file and handle based on file contents
    ifstream in_file(requestedFile);
    if (in_file) {
        //get file size
        in_file.seekg(0, ios::end);
        int fileSize = in_file.tellg();

        //get file type info in string format for response
        string contentType;
        if (type[0] == "png") {contentType = "image/png";}
        else if (type[0] == "html") {contentType = "text/html";}
        else if (type[0] == "css") {contentType = "text/css";}
        else if (type[0] == "js") {contentType = "text/javascript";}


        if (requestedFile == "shakespeare.txt") {
            contentType = "text/plain";
            http_context fileData(socketNum, requestedFile, fileSize, contentType,
                                  logRequestStr);
            handleShakespeare(fileData, partialFile);
        } else {
            http_context fileData(socketNum, requestedFile, fileSize, contentType,
                                  logRequestStr);
            handleFile(fileData);
        }
        pthread_exit(0);

    } else if (requestedFile == "quit") {
        quitProgram(logRequestStr, socketNum);

    } else if (requestedFile == "env") {
        getEnvironment(socketNum, logRequestStr);

    } else {
        fileNotFound(logRequestStr, socketNum);
        pthread_exit(0);
    }

}

//Stephen Riley's get tid function
long get_tid_xplat() {
#ifdef __APPLE__
    long   tid;
    pthread_t self = pthread_self();
    int res = pthread_threadid_np(self, reinterpret_cast<__uint64_t *>(&tid));
    return tid;
#else
    pid_t tid = gettid();
    return (long) tid;
#endif
}

//generates server response for existing files
void handleFile(http_context fileData) {

    //get header for response
    string responseHeader = getHeader(fileData.getFileSize(), fileData.getType());

    //get file contents for response body
    stringstream ssBody;
    ifstream file;
    file.open(fileData.getFileName());
    if (file) {ssBody << file.rdbuf();}
    file.close();
    string responseBody = ssBody.str();

    //write response header to socket
    write(fileData.getSocket(), responseHeader.c_str(), responseHeader.length());
    //write response body to socket
    write(fileData.getSocket(), responseBody.c_str(), responseBody.length());
    shutdown(fileData.getSocket(), SHUT_WR);
    close(fileData.getSocket());

    //generate logger response & write to pipe
    string loggerOutput = getLogger(fileData.getLogger(),
                                    fileData.getType(), fileData.getFileSize());
    pthread_mutex_lock(&lock1);
    write(pipefds[1], loggerOutput.c_str(), loggerOutput.length() + 1);
    pthread_mutex_unlock(&lock1);

    //pthread_exit(0);
}

//maps large shakespeare file to virtual memory and generates server responses
void handleShakespeare(http_context fileData, bool partialFile) {

    //Map file to virtual memory
    if (memoryMapped) {
        int fd = open("shakespeare.txt", O_RDONLY);
        mapped = static_cast<char *>(mmap(NULL, fileData.getFileSize(), PROT_READ, MAP_PRIVATE, fd, 0));
        memoryMapped = false;
    }

    //get start/length of file to read
    int start, length;
    if (partialFile) {
        string logRequest = fileData.getLogger();
        vector<string> getLength = split(logRequest,'=');
        length = stoi(getLength[2]);
        vector<string> getStart = split(getLength[1], '&');
        start = stoi(getStart[0]);
    } else {
        start = 0;
        length = fileData.getFileSize();
    }

    //get response header
    string responseHeader = getHeader(length, fileData.getType());
    //get file contents for response body
    stringstream ssBody;
    for (int i = start; i<start+length; i++) {
        ssBody << mapped[i];
    }
    string responseBody = ssBody.str();

    //write response to socket
    write(fileData.getSocket(), responseHeader.c_str(), responseHeader.length());
    write(fileData.getSocket(), responseBody.c_str(), responseBody.length());
    shutdown(fileData.getSocket(), SHUT_WR);
    close(fileData.getSocket());

    //get logger string and write to pipe
    string loggerOutput = getLogger(fileData.getLogger(), fileData.getType(), length);
    pthread_mutex_lock(&lock1);
    write(pipefds[1], loggerOutput.c_str(), loggerOutput.length() + 1);
    pthread_mutex_unlock(&lock1);
}

//generates server response for environment variables
void getEnvironment(int socketNum, string logRequestStr) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    int max_rss_mb = usage.ru_maxrss / 1024;    // Max RSS in megabytes

    //get content for response body
    string contentType = "text/html";
    stringstream html_output;
    html_output << "<h1>Environment variables</h1>\n";
    html_output << "<h2>Memory</h2>\n";
    html_output << "Max size so far: ";
    html_output << max_rss_mb;
    html_output << "<h2>Variables</h2>\n";
    char **var = environ;
    for (; *var; var++) {
        string env_var = string(*var);
        html_output << env_var <<  std::endl;
    }

    //write response header and body to socket
    string response = html_output.str();
    string responseHeader = getHeader(response.length(), contentType);
    write(socketNum, responseHeader.c_str(), responseHeader.length());
    write(socketNum, response.c_str(), response.length());
    shutdown(socketNum, SHUT_WR);
    close(socketNum);

    //get logger content and write to pipe
    string loggerOutput = getLogger(logRequestStr, contentType, response.length());
    pthread_mutex_lock(&lock1);
    write(pipefds[1], loggerOutput.c_str(), loggerOutput.length() + 1);
    pthread_mutex_unlock(&lock1);
}

//helper function to generate response header string
string getHeader(int fileSize, string contentType) {
    stringstream ssHeader;
    ssHeader << "HTTP/1.0 200 OK\r\n";
    ssHeader << "Content-Length: ";
    ssHeader << fileSize;
    ssHeader << "\r\n";
    ssHeader << "Content-Type: ";
    ssHeader << contentType;
    ssHeader << "\r\n";
    ssHeader << "Connection: Closed";
    ssHeader << "\r\n";
    ssHeader << "\r\n";
    return ssHeader.str();
}

//helper function to generate string for logger
string getLogger(string logRequestStr, string contentType, int fileSize ) {
    stringstream ssLogger;
    ssLogger << logRequestStr;
    ssLogger << "200 Ok content-type: ";
    ssLogger << contentType;
    ssLogger << " content-length: ";
    ssLogger << fileSize;
    ssLogger << "\n";
    return ssLogger.str();
}
//handles quit request from client, sends server response and ends program
void quitProgram(string logRequestStr, int socketNum) {
    //write request status to logger
    stringstream ssLogger;
    ssLogger << logRequestStr;
    ssLogger << "200 ok\n";
    string loggerOutput = ssLogger.str();
    pthread_mutex_lock(&lock1);
    write(pipefds[1], loggerOutput.c_str(), loggerOutput.length() + 1);
    pthread_mutex_unlock(&lock1);

    //generate response header and body and write to socket
    stringstream ssHeader;
    ssHeader << "HTTP/1.0 200 OK\r\n";
    ssHeader << "Content-Length: 14\r\n";
    ssHeader << "Content-Type: text/plain\r\n";
    ssHeader << "Connection: Closed";
    ssHeader << "\r\n";
    ssHeader << "\r\n";
    ssHeader << "Shutting down.\n";
    string response = ssHeader.str();
    write(socketNum, response.c_str(), response.length());
    shutdown(socketNum, SHUT_WR);
    close(socketNum);

    //end program
    string endprgrm = "Shutting down.";
    pthread_mutex_lock(&lock1);
    write(pipefds[1], endprgrm.c_str(), endprgrm.length() + 1);
    pthread_mutex_unlock(&lock1);
    pthread_mutex_destroy(&lock1);
    close(pipefds[0]);
    close(pipefds[1]);
    close(socketNum);
    //pthread_exit(0);
    sleep(1);
    exit(0);
}

void fileNotFound(string logRequestStr, int socketNum) {
    //write file note found error to logger
    stringstream ssLogger;
    ssLogger << logRequestStr;
    ssLogger << "404 File not found";
    ssLogger << "\n";
    string loggerOutput = ssLogger.str();
    pthread_mutex_lock(&lock1);
    write(pipefds[1], loggerOutput.c_str(), loggerOutput.length() + 1);
    pthread_mutex_unlock(&lock1);

    //write file not found error to socket
    stringstream ss;
    ss << "HTTP/1.0 404 File Not Found\r\n\r\n";
    string msgOut = ss.str();
    pthread_mutex_lock(&lock1);
    write(socketNum, msgOut.c_str(), msgOut.length() + 1);
    pthread_mutex_unlock(&lock1);
    close(socketNum);

    //pthread_exit(0);
}


vector<string> split(string& src, char delim) {
    istringstream ss(src);
    vector<string> res;

    string piece;
    while(getline(ss, piece, delim)) {
        res.push_back(piece);
    }

    return res;
}