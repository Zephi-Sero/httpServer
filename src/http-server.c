/* mime-types.h contains:
 *
 * typedef struct {
 *	 char const *const extension;
 *	 char const *const type;
 * } MimeType;
 *
 * MimeType const mimeTypes[] = {
 *	 {"html", "Content-Type: text/html\r\n"},
 *	 // This goes on for quite some time with various mime types
 * };
 */
#include "mime-types.h"

#include <arpa/inet.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

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


// Used for error messages
#define STRING_VALUE(X) #X

#define PORT 8080

// This limits the maximum amount of request that can be read
#define MAXIMUM_REQUEST_SIZE (1024 * 2)

// This is the limit on how long path you can request like:
// http://cool.website/path/to/file.txt
#define MAXIMUM_REQUEST_LOCATION_SIZE 1024

// OK
#define REPLY_200 "HTTP/1.0 200 OK\r\n"
// Bad Request
#define REPLY_400  \
	"HTTP/1.0 400 Bad Request\r\n" \
	"<html>\n\t<body>\n\t\t<h1>400 Bad Request</h1>\n\t</body>\n</html>"
// Not Found
#define REPLY_404 \
	"HTTP/1.0 404 Not Found\r\n\r\n" \
	"<html>\n\t<body>\n\t\t<h1>404 Not Found</h1>\n\t</body>\n</html>"
// Internal Server Error
#define REPLY_500 \
	"HTTP/1.0 500 Internal Server Error\r\n\r\n" \
	"<html>\n\t<body>\n\t\t<h1>500 Internal Server Error.</h1>\n\t\t" \
	"Please try again\n\t</body>\n</html>"
// Not Implemented
#define REPLY_501 \
	"HTTP/1.0 501 Not Implemented\r\n\r\n" \
	"<html>\n\t<body>\n\t\t<h1>501 Not Implemented</h1>\n\t</body>\n</html>"

// Sent at the end of the header the server sends to the client.
#define END "\r\n"

void trim_right_whitespace(char *const data)
{
	// The data is from a subsection of a request so limit
	// it to the maximum size of a request
	unsigned int len = strnlen(data, MAXIMUM_REQUEST_SIZE);
	unsigned int i;
	for (i = len - 1; i != UINT_MAX; i--) {
		char const ch = data[i];
		if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'))
			break;
	}
	if (i < len - 1)
		data[i + 1] = '\0';
}

char *get_mime_type(char const *const location)
{
	// Get position of file extension ("main.txt" -> ".txt")
	char const *extension = strrchr(location, '.');
	if (extension == NULL)
		// As per RFC-7231, data with an unknown type should not get a
		// Content-Type header.
		return "";
	// Skip over the . character. ("txt")
	extension += 1;

	for (unsigned int i = 0; i < sizeof(mimeTypes) / sizeof(MimeType); i++) {
		if (strncmp(extension, mimeTypes[i].extension, 64) == 0) {
			// More padding than needed but yea
			size_t const limit = strnlen(mimeTypes[i].type, 512) + 32;
			// Add an extra byte so we always have a null terminator
			char *const type = calloc(limit + 1, sizeof(char));
			strncat(type, "Content-Type: ", limit);
			strncat(type, mimeTypes[i].type, limit);
			return type;
		}
	}
	// As per RFC-7231, data with an unknown type should not get a
	// Content-Type header.
	return "";
}

void handle_connection(int const clientFD)
{
	char *const requestData = calloc(MAXIMUM_REQUEST_SIZE + 1, sizeof(char));
	if (requestData == NULL) {
		perror("handle_connection(): Failed to allocate requestData");
		send(clientFD, REPLY_500, sizeof(REPLY_500)-1, 0);
		close(clientFD);
		return;
	}

	ssize_t const status = recv(clientFD, requestData, MAXIMUM_REQUEST_SIZE, 0);
	if (status == -1) {
		perror("handle_connection(): Failed to receive data");
		send(clientFD, REPLY_500, sizeof(REPLY_500) - 1, 0);
		close(clientFD);
		free(requestData);
		return;
	}

	char *tokState = NULL;
	char *reqType = strtok_r(requestData, " ", &tokState);
	if (reqType == NULL) {
		fprintf(stderr, "handle_connection(): Malformed request type: %s\n", requestData);
		send(clientFD, REPLY_400, sizeof(REPLY_400) - 1, 0);
		close(clientFD);
		free(requestData);
		return;
	}

	bool headRequest = strncmp(reqType, "HEAD", 5) == 0;
	bool getRequest = strncmp(reqType, "GET", 4) == 0;
	if (getRequest || headRequest) {
		// Get the actual file requested:
		char *data = strtok_r(NULL, " ", &tokState);
		trim_right_whitespace(data);

		char *const location = calloc(MAXIMUM_REQUEST_LOCATION_SIZE + 1, sizeof(char));
		if (location == NULL) {
			perror("handle_connection(): Failed to allocate location");
			send(clientFD, REPLY_500, sizeof(REPLY_500) - 1, 0);
			close(clientFD);
			free(data);
			free(requestData);
			return;
		}

		if (strncmp(data, "/", 2) == 0) {
			// Redirect / to index.html
			strncat(location, "./index.html", MAXIMUM_REQUEST_LOCATION_SIZE);
		} else {
			// Prepend a ./ just incase they try doing a funny
			snprintf(location, MAXIMUM_REQUEST_LOCATION_SIZE, "./%s", data);

			// This strips any possible .. comedy
			char *temp;
			while ((temp = strstr(location, "..")) != NULL) {
				temp[0] = '.';
				temp[1] = '/';
			}

			// Check if the last character is / and redirect to that
			// directory's index.html
			if (location[strlen(location) - 1] == '/') {
				strncat(location, "./index.html", MAXIMUM_REQUEST_LOCATION_SIZE);
			}
		}

		struct stat st;
		// If stat() errors assume the file does not exist
		if (stat(location, &st) == -1) {
			perror("handle_connection(): Could not stat requested file");
			fprintf(stderr, "File requested: %s\n", location);
			send(clientFD, REPLY_404, sizeof(REPLY_404) - 1, 0);
			close(clientFD);
			free(requestData);
			free(location);
			return;
		}

		// If this returns null its most likely an
		// internal error as the stat check passed
		FILE *const file = fopen(location, "r");
		if (file == NULL) {
			perror("handle_connection(): Could not fopen requested file");
			fprintf(stderr, "File requested: %s\n", location);
			send(clientFD, REPLY_500, sizeof(REPLY_500) - 1, 0);
			close(clientFD);
			free(requestData);
			free(location);
			return;
		}

		char *buffer = NULL;
		if (!headRequest) {
			buffer = calloc((uint64_t) st.st_size + 1, sizeof(char));
			// If we cannot allocate the buffer thats an internal server error
			if (buffer == NULL) {
				perror("handle_connection(): Could not allocate file buffer");
				fprintf(stderr, "File requested: %s\n", location);
				send(clientFD, REPLY_500, sizeof(REPLY_500) - 1, 0);
				close(clientFD);
				free(requestData);
				free(location);
				return;
			}
			if (fread(buffer, sizeof(char), (size_t) st.st_size, file) != (size_t) st.st_size) {
				bool foundProblem = false;
				if (feof(file)) {
					perror("handle_connection(): fread returned shorter than stat expected");
					foundProblem = true;
				}
				if (ferror(file)) {
					perror("handle_connection(): fread errored");
					foundProblem = true;
				}
				if (!foundProblem) {
					perror("handle_connection(): fread had unknown file error without setting feof() or ferror()");
				}
				fprintf(stderr, "fread failed for file %s\n", location);
				// Clean up and don't give the client the
				// partial data, just in case they found an exploit.
				fclose(file);
				send(clientFD, REPLY_500, sizeof(REPLY_500) - 1, 0);
				close(clientFD);
				free(buffer);
				free(requestData);
				free(location);
				return;
			}
			fclose(file);
		}

		char *const mime = get_mime_type(location);
		// Reuse the location buffer to store the Content-Length header,
		// to prevent a second alloc. Assigned to new variable name so
		// it still makes sense.
		char *const header = location;
		snprintf(header, MAXIMUM_REQUEST_LOCATION_SIZE,
				"Content-Length: %lu\r\n", st.st_size);

		send(clientFD, REPLY_200, sizeof(REPLY_200) - 1, MSG_MORE);
		send(clientFD, header,    strlen(location),      MSG_MORE);
		send(clientFD, mime,      strlen(mime),          MSG_MORE);
		if (!headRequest) {
			send(clientFD, END, sizeof(END) - 1, MSG_MORE);
			send(clientFD, buffer, (size_t)st.st_size, 0);
		} else {
			send(clientFD, END, sizeof(END) - 1, 0);
		}

		close(clientFD);
		free(buffer);
		// Note that this also frees location, since they are the same
		// pointer.
		free(header);
		free(mime);
		free(requestData);
		return;
	}
	fprintf(stderr,
		"handle_connection(): Client sent a %s request, for which handling is unimplemented\n",
		reqType);
	send(clientFD, REPLY_501, sizeof(REPLY_501) - 1, 0);
	close(clientFD);
	free(requestData);
}

int main()
{
	int const socketFD = socket(AF_INET, SOCK_STREAM, 0);

	// Unfortunately required to setup the sockets.
	{
		int const optVal = 1;
		setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, (void const*)&optVal, sizeof(optVal));
	}

	struct sockaddr_in const serverAddr = {
		.sin_family = AF_INET,
		.sin_port   = htons(PORT),
		.sin_addr.s_addr = htonl(INADDR_ANY)
	};

	if (bind(socketFD, (struct sockaddr const *) &serverAddr, sizeof(serverAddr)) != 0) {
		perror("main(): Binding of socket to port failed");
		exit(1);
	}

	if (listen(socketFD, 32) != 0) {
		perror("main(): Failed to listen on port " STRING_VALUE(PORT));
		exit(1);
	}

	socklen_t clientLen = sizeof(struct sockaddr_in);

	signal(SIGCHLD, SIG_IGN);

	while (1) {
		struct sockaddr_in client;
		int const clientFD = accept(socketFD, (struct sockaddr *)&client, &clientLen);
		pid_t const pid = fork();
		if (pid == -1) {
			perror("main(): fork() errored");
			send(clientFD, REPLY_500, sizeof(REPLY_500) - 1, 0);
			close(clientFD);
		} else if (pid == 0) {
			// Child Process
			handle_connection(clientFD);
			close(socketFD);
			exit(0);
		} else {
			// Parent Process
			close(clientFD);
		}
	}

	close(socketFD);

	return 0;
}
