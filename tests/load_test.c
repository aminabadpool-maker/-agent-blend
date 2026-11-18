#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>

#define SERVER_PORT 9090
#define CONCURRENT_CLIENTS 15

void client_worker(int id) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        exit(1);
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        exit(1);
    }

    // دستور پایتون تستی برای بلندر در هر کلاینت
    char payload[256];
    snprintf(payload, sizeof(payload), "import bpy\nprint('Load Test Client #%d - Cube count:', len(bpy.data.objects))\n", id);
    
    write(sock, payload, strlen(payload));

    char buffer[1024];
    ssize_t bytes = read(sock, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        // printf("[Client %d] Response received (%ld bytes)\n", id, bytes);
    }

    close(sock);
}

int main(void) {
    printf("[*] Starting load test with %d concurrent agents...\n", CONCURRENT_CLIENTS);
    
    clock_t start = clock();

    for (int i = 0; i < CONCURRENT_CLIENTS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        }
        if (pid == 0) {
            // Child process acts as an AI agent
            client_worker(i);
            exit(0);
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < CONCURRENT_CLIENTS; i++) {
        wait(NULL);
    }

    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("[+] Load test completed! All %d concurrent tasks processed.\n", CONCURRENT_CLIENTS);
    printf("[+] Time taken: %.4f seconds\n", cpu_time_used);

    return 0;
}
