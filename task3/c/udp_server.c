#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <errno.h>

#define PACKET_SIZE 1024
#define HEADER_SIZE 8
#define DATA_SIZE (PACKET_SIZE - HEADER_SIZE)
#define TIMEOUT_SEC 2
#define MAX_RETRY 5

typedef struct {
    int32_t seq;
    char data[DATA_SIZE];
    int len;
} Packet;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <filepath>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    char *filepath = argv[2];

    int sock;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct timeval tv = {TIMEOUT_SEC, 0};
    char ready_buf[16];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("[UDP-SERVER] Listening on port %d, waiting for client READY...\n", port);

    if (recvfrom(sock, ready_buf, sizeof(ready_buf), 0,
                 (struct sockaddr *)&client_addr, &client_len) < 0) {
        perror("recvfrom ready");
        close(sock);
        return 1;
    }
    printf("[UDP-SERVER] Client ready: %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "[UDP-SERVER] Cannot open file: %s\n", filepath);
        close(sock);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);
    printf("[UDP-SERVER] Sending: %s (%ld bytes)\n", filepath, filesize);

    Packet pkt;
    int seq = 0;
    int total_packets = 0;

    while ((pkt.len = fread(pkt.data, 1, DATA_SIZE, file)) > 0) {
        pkt.seq = seq;
        int acked = 0;
        int retries = 0;

        while (!acked && retries < MAX_RETRY) {
            sendto(sock, &pkt, HEADER_SIZE + pkt.len, 0,
                   (struct sockaddr *)&client_addr, client_len);

            int32_t ack_seq;
            socklen_t addrlen = sizeof(client_addr);
            int ret = recvfrom(sock, &ack_seq, sizeof(ack_seq), 0,
                               (struct sockaddr *)&client_addr, &addrlen);
            if (ret > 0 && ack_seq == seq) {
                acked = 1;
            } else {
                retries++;
                printf("[UDP-SERVER] Retry %d for seq %d\n", retries, seq);
            }
        }

        if (!acked) {
            fprintf(stderr, "[UDP-SERVER] Failed to deliver seq %d after %d retries\n",
                    seq, MAX_RETRY);
            break;
        }

        total_packets++;
        if (seq % 10 == 0) {
            printf("[UDP-SERVER] Progress: %d/%ld bytes (seq %d)\n",
                   seq * DATA_SIZE, filesize, seq);
        }
        seq++;
    }

    pkt.seq = -1;
    pkt.len = 0;
    for (int i = 0; i < 3; i++) {
        sendto(sock, &pkt, HEADER_SIZE, 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    fclose(file);
    close(sock);
    printf("[UDP-SERVER] Done: %d packets sent\n", total_packets);
    return 0;
}
