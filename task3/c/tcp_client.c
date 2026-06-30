#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <save_path>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char *savepath = argv[3];

    int sock;
    struct sockaddr_in address;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &address.sin_addr) <= 0) {
        fprintf(stderr, "[CLIENT] Invalid IP: %s\n", server_ip);
        close(sock);
        return 1;
    }

    printf("[CLIENT] Connecting to %s:%d...\n", server_ip, port);
    if (connect(sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    printf("[CLIENT] Connected\n");

    FILE *file = fopen(savepath, "wb");
    if (!file) {
        fprintf(stderr, "[CLIENT] Cannot create file: %s\n", savepath);
        close(sock);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t n, total = 0;
    while ((n = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, n, file);
        total += n;
        printf("\r[CLIENT] Received: %zd bytes", total);
        fflush(stdout);
    }
    printf("\n[CLIENT] Done: %zd bytes -> %s\n", total, savepath);

    fclose(file);
    close(sock);

    if (n < 0) {
        perror("recv");
        return 1;
    }
    return 0;
}
