#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
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

#define PORT 8080
#define MAXIMUM_REQUEST_SIZE 1024

#define REPLY_200 "HTTP/1.0 200 OK\r\n"
#define REPLY_404 "HTTP/1.0 404 Not Found\r\n\r\n<html>\n\t<body>\n\t\t<h1>404 Not Found</h1>\n\t</body>\n</html>"
#define REPLY_501 "HTTP/1.0 501 Not Implemented\r\n\r\n<html>\n\t<body>\n\t\t<h1>500 Not Implemented</h1>\n\t</body>\n</html>"

#define START "Content-Type: "
#define END  "\r\n\r\n"

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

size_t getExtensionHash(char *location, size_t locationLen) {
    size_t hash = 0;
    size_t j = 0;
    for (int i = locationLen;i > 0;i--) {
        hash += location[i];
        if (location[i] > 111)
            j += 10;
        else
            hash *= 2;
        j++;
        if (location[i] == '.') break;
    }
    hash += j;
    return hash;
}

char *getMimeType(char *location) {
    size_t locationLen = strlen(location)-1;
    size_t hash = getExtensionHash(location,locationLen);

    /* You can add and remove items from this
     * switch statement if you do not need them.
     * Here is a python script for easily convering text
     * into the hashed data:
     *
     * #!/bin/env python3
     * import sys
     * hash= 0
     * j = 0
     * print(sys.argv[1][::-1])
     * for i in sys.argv[1][::-1]:
     *    hash += ord(i)
     *    if ord(i) > 111:
     *        j += 10
     *    else:
     *        hash *= 2
     *    j += 1
     *    if i == '.':
     *        break
     * hash += j
     * print(hash)
     *
     */

    switch (hash) {
        /* audio/.* */
        case 2044: /* .aac */
            return START"audio/aac"END;
        case 1398: /* .mp3 */
            return START"audio/mpeg"END;
        case 1214: /* .wav */
            return START"audio/wav"END;
        case 6089: /* .flac */
            return START"audio/x-flac"END;

        /* application/.* */
        case 3639: /* .json */
            return START"application/json"END;
        case 2230: /* .abw */
            return START"application/x-abiword"END;
        case 1742: /* .arc */
            return START"application/x-freearc"END;
        case 3088: /* .bin */
            return START"application/octet-stream"END;
        case 985:  /* .bz */
            return START"application/x-bzip"END;
        case 1386: /* .bz2 */
            return START"application/x-bzip2"END;
        case 2968: /* .doc */
            return START"application/msword"END;
        case 4899: /* .docx */
            return START"application/vnd.openxmlformats-officedocument.wordprocessingml.document"END;
        case 2221: /* .epub */
            return START"application/epub+zip"END;
        case 1005: /* .gz */
            return START"application/gzip"END;
        case 2218: /* .jar */
            return START"application/java-archive"END;
        case 1546: /* .pdf */
            return START"application/pdf"END;
        case 751:  /* .sh */
            return START"application/x-sh"END;
        case 1192: /* .tar */
            return START"application/x-tar"END;
        case 1228: /* .zip */
            return START"application/zip"END;
        case 1794: /* .csh */
            return START"application/x-csh"END;

        /* image/.* */
        case 3439: /* .avif */
            return START"image/avig"END;
        case 2266: /* .bmp */
            return START"image/bmp"END;
        case 3084: /* .ico */
            return START"image/vnd.microsoft.icon"END;
        case 1594: /* .png */
            return START"image/png"END;
        case 994:  /* .svg */
            return START"image/svg+xml"END;

        /* video/.* */
        case 1806: /* .avi */
            return START"video/x-msvideo"END;
        case 1486: /* .mp4 */
            return START"video/mp4"END;
        case 3447: /* .mpeg */
            return START"video/mpeg"END;

        /* text/.* */
        case 3587: /* .html */
        case 1858: /* .htm */
            return START"text/html"END;
        case 1432:  /* .css */
            return START"text/css"END;
        case 989:  /* .js */
            return START"text/javascript"END;
        case 1444: /* .csv */
            return START"text/csv"END;
        case 830:  /* .txt */
            return START"text/plain"END;

        /* umknown */
        default:
            return END;
    }
}

char *getContentLength(size_t length) {
        char *buf;
        buf = calloc(128,sizeof(char));
        sprintf(buf, "Content-Length: %lu\r\n",length);
        return buf;
}

char  *location;
char  *buffer;
size_t bufferSize;
struct stat st;
void handleConnection(size_t clientFD) {
    char *requestData = calloc(MAXIMUM_REQUEST_SIZE,sizeof(char));

    recv(clientFD, requestData, MAXIMUM_REQUEST_SIZE, 0);

    char *data = strtok(requestData," ");
    if (data == NULL) {
        free(requestData);
        close(clientFD);
        return;
    }
    if ((strcmp(data,"GET") == 0) || (strcmp(data,"HEAD") == 0)) {
        int headState = 0;
        if (strcmp(data,"HEAD") == 0)
            headState = 1;
        char *data = strtok(NULL," ");
        rstripWhitespace(data);

        if (strcmp(data,"/") == 0) {
            memmove(location,"./index.html", sizeof("./index.html"));
        } else {
            memmove(location,"./",sizeof("./"));
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

        stat(location, &st);

        FILE *file = fopen(location,"r");
        if (file == NULL) {
            send(clientFD,REPLY_404,strlen(REPLY_404),0);
            close(clientFD);
            return;
        }
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
        if (headState == 0) {
            send(clientFD,buffer,st.st_size,0);
        }

        close(clientFD);

    } else {
        send(clientFD,REPLY_501,strlen(REPLY_501),0);
        close(clientFD);
    }

    return;
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

    location   = calloc(1024,sizeof(char));
    buffer     = calloc(1024,sizeof(char));
    bufferSize = 1024;

    while (1) {
        handleConnection(accept(socketFD, (struct sockaddr*)&client, &clientLen));
    }

    close(socketFD);

    return 0;
}
