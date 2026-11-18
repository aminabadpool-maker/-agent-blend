#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>

#define SERVER_PORT 9090
#define BUFFER_SIZE 8192
#define WORKER_POOL_SIZE 4
#define QUEUE_SIZE 256
#define TASK_TIMEOUT_SEC 30

// Task Queue Structure
int task_queue[QUEUE_SIZE];
int queue_front = 0;
int queue_rear = 0;
int queue_count = 0;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

volatile sig_atomic_t server_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    server_running = 0;
    exit(0);
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';

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

    char command[1024];
    snprintf(command, sizeof(command), "timeout %d blender --background --python %s 2>&1", TASK_TIMEOUT_SEC, script_path);

    FILE *fp = popen(command, "r");
    char response[BUFFER_SIZE];
    size_t total_read = 0;
    if (fp) {
        size_t r;
        while ((r = fread(response + total_read, 1, sizeof(response) - total_read - 1, fp)) > 0) {
            total_read += r;
            if (total_read >= sizeof(response) - 1) break;
        }
        int pclose_status = pclose(fp);
        if (pclose_status != 0) {
            if (total_read < sizeof(response) - 30) {
                total_read += snprintf(response + total_read, sizeof(response) - total_read, "\n[Server Warning]: Execution timed out or failed.\n");
            }
        }
        response[total_read] = '\0';
    } else {
        strcpy(response, "Error executing blender process\n");
    }

    unlink(script_path);

    write(client_fd, response, strlen(response));
    close(client_fd);
}

void *worker_thread(void *arg) {
    (void)arg;
    while (server_running) {
        pthread_mutex_lock(&queue_lock);
        while (queue_count == 0 && server_running) {
            pthread_cond_wait(&queue_not_empty, &queue_lock);
        }

        if (!server_running) {
            pthread_mutex_unlock(&queue_lock);
            break;
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
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

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

    if (listen(server_fd, 128) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    pthread_t workers[WORKER_POOL_SIZE];
    for (int i = 0; i < WORKER_POOL_SIZE; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }

    printf("[+] -agent-blend Production Server (Workers: %d, Timeout: %ds) active on port %d...\n", 
           WORKER_POOL_SIZE, TASK_TIMEOUT_SEC, SERVER_PORT);

    while (server_running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        pthread_mutex_lock(&queue_lock);
        while (queue_count >= QUEUE_SIZE && server_running) {
            pthread_cond_wait(&queue_not_full, &queue_lock);
        }

        if (!server_running) {
            pthread_mutex_unlock(&queue_lock);
            close(client_fd);
            break;
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
