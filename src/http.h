#ifndef HTTP_H_
#define HTTP_H_
#include <sys/types.h>

typedef int fd_t;

// Функция для отправки HTTP-ответа
ssize_t send_response(fd_t client_socket,
		      char const * status,
		      char const * content_type,
		      char const * content);

size_t get_file_size(fd_t fd);

// Функция для отправки файла
ssize_t send_file(fd_t client_socket, char const * path);

ssize_t handle_request(fd_t fd, char const * request);

ssize_t send_bad_request(fd_t fd);

#endif // !HTTP_H_
