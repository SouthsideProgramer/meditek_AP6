#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <filepath>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    char *filepath = argv[2];

    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[SERVER] TCP listening on port %d...\n", port);

    client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        return 1;
    }

    printf("[SERVER] Client connected: %s:%d\n",
           inet_ntoa(address.sin_addr), ntohs(address.sin_port));

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "[SERVER] Cannot open file: %s\n", filepath);
        close(client_fd);
        close(server_fd);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);
    printf("[SERVER] Sending file: %s (%ld bytes)\n", filepath, filesize);

    char buffer[BUFFER_SIZE];
    size_t n, total = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(client_fd, buffer, n, 0) != (ssize_t)n) {
            perror("[SERVER] send error");
            break;
        }
        total += n;
    }

    fclose(file);
    close(client_fd);
    close(server_fd);
    printf("[SERVER] Done: %zu bytes sent\n", total);
    return 0;
}
