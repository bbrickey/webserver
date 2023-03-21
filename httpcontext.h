//
// http_context class
// contains data parsed from client requests needed for server response
//

#ifndef WK4TCPSERVER_HTTPCONTEXT_H
#define WK4TCPSERVER_HTTPCONTEXT_H

using namespace std;

#include <string>

class http_context {
public:
    http_context(int, string, int, string, string);

    int getSocket();
    string getFileName();
    string getLogger();
    string getType();
    int getFileSize();

    int socketNum; // FD for writing to client socket
    string requested_file; // file requested by client
    string logRequestStr; // string info for writing to logger
    string contentType; // type of file
    int fileSize; // size of file
};

//default constructor
http_context::http_context(int socket, string file, int size, string type, string log) {
    socketNum = socket;
    requested_file = file;
    logRequestStr = log;
    contentType = type;
    fileSize = size;
}

int http_context::getSocket() {
    return socketNum;
};

string http_context::getFileName() {
    return requested_file;
};

string http_context::getLogger() {
    return logRequestStr;
};

string http_context::getType() {
    return contentType;
};

int http_context::getFileSize() {
    return fileSize;
};



#endif //WK4TCPSERVER_HTTPCONTEXT_H
