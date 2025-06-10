#include "socks.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* 	Разработать простой HTTP-сервер, который сможет работать по протоколу
 * HTTP 1.0. В лабораторной работе необходимо реализовать базовую часть и
 * индивидуальное задание. К базовой части относиться поддержка всего одной
 * команды GET. Если в качестве запрашиваемого параметра указан файл,
 * который присутствует в папке программы, то он должен возвращаться
 * клиенту. Иначе – необходимо сформировать ответ с ошибкой 404 (страница не
 * найдена).
 * 	Если в качестве запроса на сервер была направлена другая команда, или
 * команда с неправильным форматом, необходимо возвращать ошибку 501 (не
 * реализовано).
 *
 * 7. Добавить ограничение клиентов. При запуске сервера необходимо
 * предусмотреть ввод названия клиента, которым разрешено подключаться к
 * серверу. Всем остальным клиентам сервер должен отвечать страницей с
 * ошибкой 400 (плохой запрос).
 */

constexpr in_port_t PORT = 8789;
[[maybe_unused]] constexpr uint32_t LOCALHOST = (127 << 24) + 1;

int communication_cycle(fd_t fd, char const * valid_ip);

// Функция для отправки HTTP-ответа
ssize_t send_response(int client_socket,
		      char const * status,
		      char const * content_type,
		      char const * content);

int main()
{
	// Структура с адресом и портом сервера
	struct sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	// server_addr.sin_addr = (struct in_addr){htonl(LOCALHOST)};
	server_addr.sin_addr = (struct in_addr){htonl(0)};

	// Входящий сокет
	fd_t serv_sock = create_bind_server_socket(&server_addr);
	if (-1 == serv_sock) {
		perror("Failed to create binded socket");
		return -1;
	}

	// Чтение корректного IP-адреса
	char valid_ip[19];
	do {
		printf("Введите валидный IP-адрес: ");
		if (scanf("%18s", valid_ip) < 0) {
			close(serv_sock);
			return 0;
		};
		while (getchar() != '\n')
			;

	} while (inet_addr(valid_ip) == (uint32_t)-1);

	printf("Ожидание соединения на порт %hu\n", PORT);

	int communication_cycle_bad = communication_cycle(serv_sock, valid_ip);
	if (communication_cycle_bad < 0) {
		perror("Recv failed");
		close(serv_sock);
		return 1;
	}

	// Штатное завершение работы
	puts("Клиент прервал соединение");
	close(serv_sock);
	return 0;
}

// Функция для отправки HTTP-ответа
ssize_t send_response(int client_socket,
		      char const * status,
		      char const * content_type,
		      char const * content)
{
	constexpr size_t BUFFER_SIZE = 640;
	char response[BUFFER_SIZE];

	snprintf(response, sizeof(response),
		 "HTTP/1.0 %s\r\n"
		 "Content-Type: %s\r\n"
		 "Content-Length: %zu\r\n"
		 "\r\n"
		 "%s",
		 status, content_type, strlen(content), content);

	return write(client_socket, response, strlen(response));
}

static inline ssize_t send_not_found(fd_t fd)
{
		return send_response(fd, "404 Not Found",
				     "text/html", "<h1>404 Not Found</h1>");
}
static inline ssize_t send_not_implemented(fd_t fd)
{
	return send_response(fd, "501 Not Implemented", "text/html",
			     "<h1>501 Not Implemented</h1>");
}

static inline ssize_t send_bad_request(fd_t fd)
{
	return send_response(fd, "400 Bad Request", "text/html",
			     "<p>400 Bad Request</p>");
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
ssize_t send_file(int client_socket, char const * path)
{
	++path; // Смещение символа /

	int fd = open(path, O_RDONLY);
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
		if(send(client_socket, buffer, (size_t)bytes_read, 0) < 0){
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
	if (!path_end || strncmp(path_end, " HTTP/1.1", 9) != 0) {
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

int communication_cycle(fd_t serv_sock, char const * valid_ip)
{
	constexpr size_t buflen = 64;
	char buf[buflen + 1];
	buf[buflen] = '\0';

	do {
		fd_t client_sock;
		// Структура с адресом и портом клиента
		struct sockaddr_in client_addr = {};
		socklen_t client_addr_len = sizeof(client_addr);

		client_sock = accept(serv_sock, (struct sockaddr *)&client_addr,
				     &client_addr_len);
		if (client_sock < 0) {
			return -1;
		}
		print_sockaddr_in_info(&client_addr);

		ssize_t recv_ret = recv(client_sock, buf, buflen, 0);
		if (recv_ret < 0) {
			close(client_sock);
			return -1;
		} else if (recv_ret == 0) { // Пустой запрос
			close(client_sock);
			return 0;
			// continue;
		}
		// else if (recv_ret > 0)
		buf[recv_ret] = '\0';

		bool const bad_client =
			strcmp(valid_ip, inet_ntoa(client_addr.sin_addr));
		if (bad_client) {
			if (send_bad_request(client_sock) < 0) {
				close(client_sock);
				return -1;
			}
			close(client_sock);
			continue;
		}

		// Зануление переноса строки
		char * endl = strpbrk(buf, "\r\n");
		if (endl)
			*endl = '\0';

		ssize_t sent_bytes = handle_request(client_sock, buf);
		if (sent_bytes < 0) {
			close(client_sock);
			return -1;
		} else
			close(client_sock);

	} while (true);
}
