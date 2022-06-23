#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>

/* This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 */

#include "mimeTypes.h"

#define PORT 8080
#define MAXIMUM_REQUEST_SIZE 1024

#define REPLY_200 "HTTP/1.0 200 OK\r\n"
#define REPLY_404 "HTTP/1.0 404 Not Found\r\n\r\n<html>\n\t<body>\n\t\t<h1>404 Not Found</h1>\n\t</body>\n</html>"
#define REPLY_501 "HTTP/1.0 501 Not Implemented\r\n\r\n<html>\n\t<body>\n\t\t<h1>500 Not Implemented</h1>\n\t</body>\n</html>"

#define END  "\r\n"

void rstripWhitespace(char *data) {
    int i = strlen(data)-1;
    char t = data[i];
    if ((t == ' ' || t == '\r' || t == '\n')) {
        int cont = 1;
        while (cont) {
            switch (data[i]) {
                case ' ':
                case '\r':
                case '\n':
                    data[i] = 0;
                    i--;
                    break;

                default:
                    cont = 0;
                    break;
            }
        }
    }
}

char *getMimeType(char *location) {
    int locationLen = strlen(location);

    char *output = calloc(locationLen,sizeof(char));
    int j = 0;
    for (int i = locationLen-1;i > 0;i--) {
        output[j] = location[i];
        j++;
        if (location[i] == '.') break;
    }


    int outputLen = strlen(output);
    int end = outputLen/2;
    j = outputLen-1;
    char temp;
    for (int i = 0;i < end;i++) {
        temp = output[j];
        output[j] = output[i];
        output[i] = temp;
        j--;
    }

    for (int i = 0;i < sizeof(mTypes)/sizeof(mimeType);i++) {
        if (strcmp(output,mTypes[i].Extension) == 0) {
            free(output);
            return (char *)mTypes[i].Type;
        }
    }
    free(output);
    return "";
}

char *getContentLength(size_t length) {
    char *buf = calloc(256,sizeof(char));
    sprintf(buf, "Content-Length: %lu\r\n",length);
    return buf;
}


void handleConnection(size_t clientFD) {
    char *requestData = calloc(MAXIMUM_REQUEST_SIZE,sizeof(char));

    size_t bufferSize = 1024;

    recv(clientFD, requestData, MAXIMUM_REQUEST_SIZE, 0);

    char *data = strtok(requestData," ");
    if (data == NULL) {
        send(clientFD,REPLY_404,strlen(REPLY_404),0);
        free(requestData);
        close(clientFD);
        exit(1);
    }
    if ((strcmp(data,"GET") == 0) || (strcmp(data,"HEAD") == 0)) {
        int headState = 0;
        if (strcmp(data,"HEAD") == 0)
            headState = 1;
        char *data = strtok(NULL," ");
        rstripWhitespace(data);

        char  *location = calloc(1024,sizeof(char));
        if (strcmp(data,"/") == 0) {
            memmove(location,"./index.html", strlen("./index.html"));
        } else {
            memmove(location,"./",strlen("./"));
            strcat(location,data);

            for (size_t i = 0;i < strlen(location);i++) {
                if (location[i] == '.' && location[i+1] == '.') {
                    location[i]   = '/';
                    location[i+1] = '/';
                }
            }
            if (location[strlen(location)-1] == '/') {
                strcat(location,"./index.html");
            }
        }

        struct stat st;
        stat(location, &st);

        FILE *file = fopen(location,"r");
        if (file == NULL) {
            send(clientFD,REPLY_404,strlen(REPLY_404),0);
            close(clientFD);
            free(requestData);
            free(location);
            exit(1);
        }
        char  *buffer = calloc(1024*8,sizeof(char));
        if (bufferSize < st.st_size) {
            bufferSize += st.st_size;
            buffer = realloc(buffer,bufferSize);
        }
        fread(buffer,sizeof(char),st.st_size,file);
        fclose(file);

        char *mime = getMimeType(location);
        char *leng = getContentLength(st.st_size);
        send(clientFD,REPLY_200,strlen(REPLY_200),MSG_MORE);
        send(clientFD,leng,strlen(leng),MSG_MORE);
        send(clientFD,mime,strlen(mime),MSG_MORE);
        send(clientFD,END,strlen(END),MSG_MORE);
        if (headState == 0) {
            send(clientFD,buffer,st.st_size,0);
        }
        free(leng);
        free(buffer);
        free(location);
        close(clientFD);

    } else {
        send(clientFD,REPLY_501,strlen(REPLY_501),0);
        close(clientFD);
    }
    free(requestData);
    exit(0);
}

int main() {


    int socketFD;
    struct sockaddr_in serverAddr;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
  
    { /* I do not wish to dirty my code with this but alas it is needed */
        int optVal = 1;
        setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, (void *)&optVal, sizeof(optVal));
    }

    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_port        = htons(PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((bind(socketFD, (struct sockaddr*)&serverAddr, sizeof(serverAddr))) != 0) {
        perror("Socket binding failed");
        exit(1);
    }

    if ((listen(socketFD,16)) != 0) {
        perror("Failed to listen");
        exit(1);
    }

    struct sockaddr_in client;
    socklen_t clientLen = sizeof(client);

    /* Ignore when a child exits as we do not care about its return value */
    signal(SIGCHLD,SIG_IGN);

    while (1) {
        size_t clientFD = accept(socketFD, (struct sockaddr*)&client, &clientLen);
        
        pid_t pid;
        if ((pid = fork()) == 0) {
            handleConnection(clientFD);
        } else if (pid != -1) {
            close(clientFD);
        } else {
            perror("Fork: ");
            exit(1);
        }
        
        //handleConnection(clientFD);
    }

    close(socketFD);

    return 0;
}
