#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "blend_protocol.h"

#define DEFAULT_PORT 9090
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command_or_script_path> [server_ip]\n", argv[0]);
        return 1;
    }

    const char *cmd_payload = argv[1];
    const char *server_ip = (argc > 2) ? argv[2] : "127.0.0.1";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to agent-blend server failed");
        close(sock);
        return 1;
    }

    // ارسال دستور به سرور
    write(sock, cmd_payload, strlen(cmd_payload));

    // دریافت پاسخ از سرور
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = read(sock, buffer, sizeof(buffer) - 1);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("[Server Response]:\n%s\n", buffer);
    }

    close(sock);
    return 0;
}
