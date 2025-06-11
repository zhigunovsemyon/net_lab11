#include "http.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Функция для отправки HTTP-ответа
ssize_t send_response(fd_t client_socket,
		      char const * status,
		      char const * content_type,
		      char const * content)
{
	constexpr size_t BUFFER_SIZE = 120;
	char response[BUFFER_SIZE];

	snprintf(response, sizeof(response),
		 "HTTP/1.0 %s\r\n"
		 "Content-Type: %s\r\n"
		 "Content-Length: %zu\r\n"
		 "\r\n"
		 "%s",
		 status, content_type, strlen(content), content);

	return send(client_socket, response, strlen(response),0);
}

static inline ssize_t send_not_found(fd_t fd)
{
	return send_response(fd, "404 Not Found", "text/html",
			     "<h1>404 Not Found</h1>");
}

static inline ssize_t send_not_implemented(fd_t fd)
{
	return send_response(fd, "501 Not Implemented", "text/html",
			     "<h1>501 Not Implemented</h1>");
}

ssize_t send_bad_request(fd_t fd)
{
	return send_response(fd, "400 Bad Request", "text/html",
			     "<h1>400 Bad Request</h1>");
}

size_t get_file_size(fd_t fd)
{
	off_t currentPos = lseek(fd, 0, SEEK_CUR);
	ssize_t size = lseek(fd, 0, SEEK_END);
	lseek(fd, currentPos, SEEK_SET);
	assert(size >= 0);
	return (size_t)size;
}

// Функция для отправки файла
ssize_t send_file(fd_t client_socket, char const * path)
{
	++path; // Смещение символа /

	fd_t fd = open(path, O_RDONLY);
	if (fd == -1) {
		return send_not_found(client_socket);
	}

	constexpr size_t BUFFER_SIZE = 100;

	// Отправляем заголовок
	char header[BUFFER_SIZE];
	snprintf(header, sizeof(header),
		 "HTTP/1.0 200 OK\r\n"
		 "Content-Type: text/plain\r\n"
		 "Content-Length: %zu\r\n"
		 "\r\n",
		 get_file_size(fd));
	if (send(client_socket, header, strlen(header), 0) < 0) {
		close(fd);
		return -1;
	}

	// Отправляем содержимое файла
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read;
	while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
		if (send(client_socket, buffer, (size_t)bytes_read, 0) < 0) {
			close(fd);
			return -1;
		}
	}

	close(fd);
	return 1;
}

ssize_t handle_request(fd_t fd, char const * request)
{
	printf("%s\n", request);
	constexpr size_t PATH_MAXLEN = 100;
	char path[PATH_MAXLEN];
	if (strncmp(request, "GET ", 4) != 0) {
		return send_not_implemented(fd);
	}

	// Извлекаем путь
	char * path_start = strchr(request, ' ') + 1;
	char * path_end = strchr(path_start, ' ');
	if (!path_end || strncmp(path_end, " HTTP/1.", 8) != 0) {
		return send_not_implemented(fd);
	}

	ssize_t path_len = path_end - path_start;
	assert(path_len >= 0);
	if ((size_t)path_len > PATH_MAXLEN)
		return send_not_found(fd);

	strncpy(path, path_start, (size_t)path_len);
	path[path_len] = '\0';

	return send_file(fd, path);
}

