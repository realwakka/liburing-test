#include <assert.h>
#include <liburing/io_uring.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <liburing.h>

#define BUFFER_SIZE 1024

enum client_state {
	ACCEPTED,
	RECEIVED,
	SENDED,
	CLOSED
};

struct client {
	enum client_state state;
	int client_socket;
	char buffer[BUFFER_SIZE];
};

int main() {
    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	int ret;

	ret = io_uring_queue_init(512, &ring, 0);
	assert(ret == 0);
	
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = 0;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

	socklen_t len = sizeof(server_addr);
	if (getsockname(server_socket, (struct sockaddr *)&server_addr, &len) < 0) {
        perror("getsockname");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
	
    printf("Server is running on http://localhost:%d\n", ntohs(server_addr.sin_port));

	sqe = io_uring_get_sqe(&ring);
	io_uring_sqe_set_data(sqe, 0);
	io_uring_prep_multishot_accept(sqe, server_socket, (struct sockaddr *)&client_addr, &addr_len, 0);
	ret = io_uring_submit(&ring);

    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello, World!";

	int accepted = 0;
	int closed = 0;
    while (1) {
		ret = io_uring_wait_cqe(&ring, &cqe);
		if (cqe->user_data == 0) { // ACCEPTED
			accepted++;
			struct client *c = malloc(sizeof(struct client));
			c->client_socket = cqe->res;
			c->state = RECEIVED;
			io_uring_cqe_seen(&ring, cqe);

			sqe = io_uring_get_sqe(&ring);
			io_uring_sqe_set_data(sqe, c);
			io_uring_prep_recv(sqe, c->client_socket, c->buffer, 1024, 0);
			io_uring_submit(&ring);
			
		} else {
			struct client *c = (struct client*)cqe->user_data;
			if (c->state == RECEIVED) {
				c->state = SENDED;
				printf("received %s\n", c->buffer);
				sqe = io_uring_get_sqe(&ring);
				io_uring_sqe_set_data(sqe, c);
				io_uring_prep_send(sqe, c->client_socket, response, strlen(response), 0);
				io_uring_submit(&ring);
			} else if (c->state == SENDED) {
				c->state = CLOSED;
				sqe = io_uring_get_sqe(&ring);
				io_uring_sqe_set_data(sqe, c);
				io_uring_prep_close(sqe, c->client_socket);
				io_uring_submit(&ring);
			} else {
				c->state = CLOSED;
				closed++;
				free(c);
			}
			io_uring_cqe_seen(&ring, cqe);			
		}
    }

    close(server_socket);
    return 0;
}
