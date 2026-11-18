#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "blend_protocol.h"

#define SERVER_PORT 9090
#define BUFFER_SIZE 4096
#define WORKER_POOL_SIZE 4
#define QUEUE_SIZE 256

// Task Queue Structure
int task_queue[QUEUE_SIZE];
int queue_front = 0;
int queue_rear = 0;
int queue_count = 0;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';

    // ایجاد فایل موقت برای اجرای امن توسط بلندر
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

    // اجرای بلندر در حالت Headless
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

    unlink(script_path);

    write(client_fd, response, strlen(response));
    close(client_fd);
}

// Worker Thread Function
void *worker_thread(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&queue_lock);
        while (queue_count == 0) {
            pthread_cond_wait(&queue_not_empty, &queue_lock);
        }

        int client_fd = task_queue[queue_front];
        queue_front = (queue_front + 1) % QUEUE_SIZE;
        queue_count--;

        pthread_cond_signal(&queue_not_full);
        pthread_mutex_unlock(&queue_lock);

        handle_client(client_fd);
    }
    return NULL;
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

    if (listen(server_fd, 64) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    // راه‌اندازی استخر کارگران (Worker Pool)
    pthread_t workers[WORKER_POOL_SIZE];
    for (int i = 0; i < WORKER_POOL_SIZE; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }

    printf("[+] -agent-blend server daemon (Worker Pool: %d workers) active on port %d...\n", WORKER_POOL_SIZE, SERVER_PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        pthread_mutex_lock(&queue_lock);
        while (queue_count >= QUEUE_SIZE) {
            pthread_cond_wait(&queue_not_full, &queue_lock);
        }

        task_queue[queue_rear] = client_fd;
        queue_rear = (queue_rear + 1) % QUEUE_SIZE;
        queue_count++;

        pthread_cond_signal(&queue_not_empty);
        pthread_mutex_unlock(&queue_lock);
    }

    close(server_fd);
    return 0;
}
