#!/usr/bin/env bash
set -e

echo "[*] Updating src/server.c with real Headless Blender integration..."
cat << 'SERVER_EOF' > src/server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "blend_protocol.h"

#define SERVER_PORT 9090
#define BUFFER_SIZE 4096

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';

    // ایجاد یک فایل اسکریپت موقت برای اجرای امن توسط بلندر
    char script_path[] = "/tmp/agent_blend_task_XXXXXX.py";
    int fd = mkstemp(script_path);
    if (fd != -1) {
        write(fd, buffer, strlen(buffer));
        close(fd);
    } else {
        const char *err_msg = "Error creating temp script file\n";
        write(client_fd, err_msg, strlen(err_msg));
        close(client_fd);
        return;
    }

    // ساخت دستور اجرای بلندر در حالت Headless
    char command[512];
    snprintf(command, sizeof(command), "blender --background --python %s 2>&1", script_path);

    FILE *fp = popen(command, "r");
    char response[BUFFER_SIZE];
    size_t total_read = 0;
    if (fp) {
        size_t r;
        while ((r = fread(response + total_read, 1, sizeof(response) - total_read - 1, fp)) > 0) {
            total_read += r;
            if (total_read >= sizeof(response) - 1) break;
        }
        pclose(fp);
        response[total_read] = '\0';
    } else {
        strcpy(response, "Error executing blender process\n");
    }

    // پاکسازی فایل موقت
    unlink(script_path);

    // ارسال خروجی بلندر به کلاینت (ایجنت)
    write(client_fd, response, strlen(response));
    close(client_fd);
}

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    printf("[+] -agent-blend server daemon with Blender backend active on port %d...\n", SERVER_PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd >= 0) {
            handle_client(client_fd);
        }
    }

    close(server_fd);
    return 0;
}
SERVER_EOF

echo "[*] Compiling binaries..."
make clean
make

echo "[*] Verifying compiled binaries..."
if [ -x ./agent_blend_server ] && [ -x ./agent_blend_cli ]; then
    echo "[+] Build check PASSED successfully!"
else
    echo "[-] Build check FAILED!"
    exit 1
fi

echo "[*] Pushing changes to GitHub..."
git add src/server.c deploy_blender.sh
git commit -m "Integrate real Headless Blender execution backend into server daemon"
git push origin main

echo "[+] Deployment & GitHub sync completed successfully!"
