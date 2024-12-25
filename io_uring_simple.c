#include <assert.h>
#include <liburing/io_uring.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <liburing.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read < 0) {
        perror("read");
        close(client_socket);
        return;
    }

    buffer[bytes_read] = '\0'; // Null-terminate the buffer

    // Simple HTTP response
    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello, World!";

    write(client_socket, response, strlen(response));
    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	int ret;

	ret = io_uring_queue_init(512, &ring, 0);
	assert(ret == 0);
	
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Bind address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = 0;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
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
	io_uring_prep_multishot_accept(sqe, server_socket, (struct sockaddr *)&client_addr, &addr_len, 0);
	ret = io_uring_submit(&ring);

    while (1) {
		ret = io_uring_wait_cqe(&ring, &cqe);
		assert(cqe->res > 0);
		client_socket = cqe->res;
		io_uring_cqe_seen(&ring, cqe);
        if (client_socket < 0) {
            perror("accept");
			printf("%d\n", cqe->res);
            continue;
        }

		//pthread_t th;
		// pthread_create(&th, NULL, handle_client, (void*)client_socket);
        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}
