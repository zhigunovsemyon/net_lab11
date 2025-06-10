#include "socks.h"
#include <assert.h>
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

int communication_cycle(fd_t fd);

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
	printf("Ожидание соединения на порт %hu\n", PORT);

	int communication_cycle_bad = communication_cycle(serv_sock);
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

static inline ssize_t send_bad_request(fd_t fd)
{
	char const * NOT_FOUND_RESPONSE = "No such entry!\n";
	return send(fd, NOT_FOUND_RESPONSE, strlen(NOT_FOUND_RESPONSE), 0);
}

ssize_t handle_request(fd_t fd, [[maybe_unused]] char const * request)
{
	// constexpr size_t sendbuf_size = 40;
	// char sendbuf[sendbuf_size + 1] = {};

	return send_bad_request(fd);
}

int communication_cycle(fd_t serv_sock)
{
	constexpr size_t buflen = 64;
	char buf[buflen + 1];
	buf[buflen] = '\0';

	fd_t client_sock;
	do {
		// Структура с адресом и портом клиента
		struct sockaddr_in client_addr = {};
		socklen_t client_addr_len = sizeof(client_addr);

		client_sock = accept(serv_sock, (struct sockaddr *)&client_addr,
				     &client_addr_len);
		if (client_sock < 0) {
			perror("Accept failed");
			close(serv_sock);
			return 1;
		}
		print_sockaddr_in_info(&client_addr);

		ssize_t recv_ret = recv(client_sock, buf, buflen, 0);
		if (recv_ret == 0) {
			break;
		} else if (recv_ret < 0)
			return -1;

		// else if (recv_ret > 0)
		buf[recv_ret] = '\0';

		// Зануление переноса строки
		char * endl = strpbrk(buf, "\r\n");
		if (endl)
			*endl = '\0';

		ssize_t sent_bytes = handle_request(client_sock, buf);
		if (sent_bytes > 0)
			continue;
		else if (sent_bytes == 0)
			break;
		// if error
		close(client_sock);
		return -1;

	} while (true);
	close(client_sock);
	return 0;
}
