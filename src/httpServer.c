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
/* mimeTypes.h contains:
 *
 * typedef struct {
 *	 char const *const Extension;
 *	 char const *const Type;
 * } mimeType;
 *
 * const mimeType mTypes[] = {
 *	 {"html","Content-Type: text/html\r\n"},
 *	 // This goes on for quite some time with various mime types
 * };
 *
 */

#define PORT 8080
/* This limits the maximum amount of request that can be read */
#define MAXIMUM_REQUEST_SIZE 1024*2
/* This is the limit on how long path you can request like:
 * http://cool.website/path/to/file.txt
 */
#define MAXIMUM_REQUEST_LOCATION_SIZE 1024

#define REPLY_200 "HTTP/1.0 200 OK\r\n"
#define REPLY_404 "HTTP/1.0 404 Not Found\r\n\r\n<html>\n\t<body>\n\t\t<h1>404 Not Found</h1>\n\t</body>\n</html>"
#define REPLY_501 "HTTP/1.0 501 Not Implemented\r\n\r\n<html>\n\t<body>\n\t\t<h1>500 Not Implemented</h1>\n\t</body>\n</html>"

#define END  "\r\n"

void rstripWhitespace(char *data) {
	unsigned int i = strlen(data);
	char t = data[i-1];
	if ((t == ' ') || (t == '\r') || (t == '\n')) {
		int cont = 1;
		while (cont) {
			switch (data[i]) {
				case ' ':
				case '\r':
				case '\n':
					i--;
					break;

				default:
					cont = 0;
					break;
			}
		}
		data[i-1] = 0;
	}
}

char *getMimeType(char *location) {

	char *output = strrchr(location, '.')+1;
	if (output == NULL)
		return "";

	for (unsigned int i = 0;i < sizeof(mTypes)/sizeof(mimeType);i++) {
		if (strcmp(output,mTypes[i].Extension) == 0) {
			size_t limit = strlen(mTypes[i].Type)+strlen("Content-Type: ")+1;
			char *type = calloc(limit,sizeof(char));
			strncat(type,"Content-Type: ",limit);
			strncat(type,mTypes[i].Type,limit);
			return type;
		}
	}
	return "";
}

void handleConnection(int clientFD) {
	char *requestData = calloc(MAXIMUM_REQUEST_SIZE,sizeof(char));

	recv(clientFD, requestData, MAXIMUM_REQUEST_SIZE, 0);

	char *data = strtok(requestData," ");
	if (data == NULL) {
		send(clientFD,REPLY_404,strlen(REPLY_404),0);
		free(requestData);
		close(clientFD);
		return;
	}
	if ((strcmp(data,"GET") == 0) || (strcmp(data,"HEAD") == 0)) {
		short int headState = 0;
		if (strcmp(data,"HEAD") == 0)
			headState = 1;
		data = strtok(NULL," ");
		rstripWhitespace(data);

		char  *location = calloc(MAXIMUM_REQUEST_LOCATION_SIZE,sizeof(char));
		if (strcmp(data,"/") == 0) {
			/* Redirect / to index.html */
			strncat(location,"./index.html", MAXIMUM_REQUEST_LOCATION_SIZE);
		} else {
			/* Prepend a ./ just incase they try doing a funny */
			snprintf(location,MAXIMUM_REQUEST_LOCATION_SIZE, "./%s",data);

			/* This strips any possible .. comedy */
			char *temp;
			while ((temp = strstr(location,"..")) != NULL) {
				temp[0] = '.';
				temp[1] = '/';
			}

			/* Check if the last character is / and redirect to index.html */
			if (location[strlen(location)] == '/') {
				strncat(location,"./index.html", MAXIMUM_REQUEST_LOCATION_SIZE);
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
			return;
		}
		char *buffer = NULL;
		if (headState == 0) {
			buffer = calloc((uint64_t)st.st_size+1,sizeof(char));
			fread(buffer,sizeof(char),(size_t)st.st_size,file);
			fclose(file);
		}

		char *mime = getMimeType(location);
		/* Reuse the location buffer to store the Content-Length header */
		snprintf(location, MAXIMUM_REQUEST_LOCATION_SIZE,"Content-Length: %lu\r\n",st.st_size);

		send(clientFD,REPLY_200,strlen(REPLY_200),MSG_MORE);
		send(clientFD,location,strlen(location),MSG_MORE);
		send(clientFD,mime,strlen(mime),MSG_MORE);
		if (headState == 0) {
			send(clientFD,END,strlen(END),MSG_MORE);
			send(clientFD,buffer,(size_t)st.st_size,0);
		} else {
			send(clientFD,END,strlen(END),0);
		}
		free(buffer);
		free(location);
		free(mime);
		close(clientFD);

	} else {
		send(clientFD,REPLY_501,strlen(REPLY_501),0);
		close(clientFD);
	}
	free(requestData);
	close(clientFD);
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

	if ((listen(socketFD,32)) != 0) {
		perror("Failed to listen");
		exit(1);
	}

	struct sockaddr_in client;
	socklen_t clientLen = sizeof(client);

	signal(SIGCHLD,SIG_IGN);

	while (1) {
		int clientFD = accept(socketFD, (struct sockaddr*)&client, &clientLen);
		pid_t pid;
		if ((pid = fork()) == 0) {
			handleConnection(clientFD);
			exit(0);
		} else if (pid != -1) {
			close(clientFD);
		} else {
			perror("Fork: ");
			exit(1);
		}
	}

	close(socketFD);

	return 0;
}
