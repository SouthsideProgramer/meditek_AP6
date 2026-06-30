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
#define TIMEOUT_SEC 5

typedef struct {
    int32_t seq;
    char data[DATA_SIZE];
    int len;
} Packet;

typedef struct PktNode {
    int32_t seq;
    char data[DATA_SIZE];
    int len;
    struct PktNode *next;
} PktNode;

static PktNode *pkt_list = NULL;

static void store_packet(int32_t seq, const char *data, int len) {
    PktNode *node = malloc(sizeof(PktNode));
    node->seq = seq;
    node->len = len;
    memcpy(node->data, data, len);
    node->next = pkt_list;
    pkt_list = node;
}

static int find_and_write(int32_t expected, FILE *file) {
    PktNode **pp = &pkt_list;
    while (*pp) {
        if ((*pp)->seq == expected) {
            PktNode *match = *pp;
            fwrite(match->data, 1, match->len, file);
            *pp = match->next;
            free(match);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

static void free_all_packets() {
    PktNode *p = pkt_list;
    while (p) {
        PktNode *tmp = p;
        p = p->next;
        free(tmp);
    }
    pkt_list = NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <save_path>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char *savepath = argv[3];

    int sock;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    struct timeval tv = {TIMEOUT_SEC, 0};

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "[UDP-CLIENT] Invalid IP: %s\n", server_ip);
        close(sock);
        return 1;
    }

    printf("[UDP-CLIENT] Sending READY to %s:%d...\n", server_ip, port);
    sendto(sock, "READY", 5, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

    FILE *file = fopen(savepath, "wb");
    if (!file) {
        fprintf(stderr, "[UDP-CLIENT] Cannot create file: %s\n", savepath);
        close(sock);
        return 1;
    }

    Packet pkt;
    int32_t expected_seq = 0;
    int total_bytes = 0;
    int done = 0;

    while (!done) {
        int n = recvfrom(sock, &pkt, sizeof(Packet), 0,
                         (struct sockaddr *)&server_addr, &addr_len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[UDP-CLIENT] Timeout - lost packets? Flushing %d buffered packets\n",
                       expected_seq);
            }
            break;
        }

        if (n < HEADER_SIZE) continue;

        if (pkt.seq == -1) {
            printf("[UDP-CLIENT] Received EOF\n");
            done = 1;
            break;
        }

        int data_len = n - HEADER_SIZE;
        if (data_len > DATA_SIZE) data_len = DATA_SIZE;

        store_packet(pkt.seq, pkt.data, data_len);

        int32_t ack = pkt.seq;
        sendto(sock, &ack, sizeof(ack), 0,
               (struct sockaddr *)&server_addr, addr_len);

        while (find_and_write(expected_seq, file)) {
            total_bytes += DATA_SIZE;
            expected_seq++;
        }
    }

    while (find_and_write(expected_seq, file)) {
        total_bytes += DATA_SIZE;
        expected_seq++;
    }

    free_all_packets();
    fclose(file);
    close(sock);
    printf("[UDP-CLIENT] Done: received seq up to %d -> %s\n", expected_seq, savepath);
    return 0;
}
