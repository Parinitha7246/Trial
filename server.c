#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_MSG 256
#define BASE_FIFO_DIR "/tmp/chat_pipes"

struct Client {
    char id[50];
    int rfd;
    int wfd;
    int active;
};

struct Client* clients = NULL;
struct pollfd* fds = NULL;
int client_count = 0;

void ensure_dir_exists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0777) == -1) {
            perror("mkdir failed");
            exit(EXIT_FAILURE);
        }
    }
}

// Create a FIFO at a specific path
void create_fifo(const char *path) {
    unlink(path); // Remove if already exists
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
}

// Check if a client ID is already registered and active
int is_duplicate_client(const char* client_id) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].active && strcmp(clients[i].id, client_id) == 0) {
            return 1;
        }
    }
    return 0;
}

// Broadcast message to all clients except the sender
void broadcast(const char *msg, const char *sender_id) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].active && strcmp(clients[i].id, sender_id) != 0) {
            write(clients[i].wfd, msg, strlen(msg));
        }
    }
}

// Add a new client dynamically
void add_client(const char* client_id) {
    clients = realloc(clients, sizeof(struct Client) * (client_count + 1));
    fds = realloc(fds, sizeof(struct pollfd) * (client_count + 2)); // +1 for reg_fd

    char to_server[100], to_client[100];
    snprintf(to_server, sizeof(to_server), "%s/%s_to_server", BASE_FIFO_DIR, client_id);
    snprintf(to_client, sizeof(to_client), "%s/server_to_%s", BASE_FIFO_DIR, client_id);

    create_fifo(to_server);
    create_fifo(to_client);

    int rfd = open(to_server, O_RDONLY | O_NONBLOCK);
    int wfd = open(to_client, O_WRONLY);

    if (rfd < 0 || wfd < 0) {
        perror("open client FIFO");
        return;
    }

    write(wfd, "READY\n", 6);

    strcpy(clients[client_count].id, client_id);
    clients[client_count].rfd = rfd;
    clients[client_count].wfd = wfd;
    clients[client_count].active = 1;

    fds[client_count + 1].fd = rfd;
    fds[client_count + 1].events = POLLIN;

    printf("[SERVER] Added client: %s\n", client_id);
    client_count++;
}

int main() {
    // Step 1: Ensure the base directory exists
    ensure_dir_exists(BASE_FIFO_DIR);

    // Step 2: Create and open the registration FIFO
    char reg_fifo_path[100];
    snprintf(reg_fifo_path, sizeof(reg_fifo_path), "%s/registration_fifo", BASE_FIFO_DIR);
    create_fifo(reg_fifo_path);

    int reg_fd = open(reg_fifo_path, O_RDONLY | O_NONBLOCK);
    if (reg_fd < 0) {
        perror("open registration_fifo");
        exit(EXIT_FAILURE);
    }

    fds = malloc(sizeof(struct pollfd));
    fds[0].fd = reg_fd;
    fds[0].events = POLLIN;

    printf("[SERVER] Running... Waiting for clients.\n");

    char buffer[MAX_MSG];

    while (1) {
        int ret = poll(fds, client_count + 1, 1000);
        if (ret < 0) {
            perror("poll");
            continue;
        }

        // Step 3: Handle new registrations
        if (fds[0].revents & POLLIN) {
            char client_id[50];
            int bytes = read(reg_fd, client_id, sizeof(client_id) - 1);
            if (bytes > 0) {
                client_id[bytes] = '\0';

                if (is_duplicate_client(client_id)) {
                    printf("[SERVER] Duplicate client '%s' ignored.\n", client_id);
                    continue;
                }

                add_client(client_id);
            }
        }

        // Step 4: Handle client messages
        for (int i = 0; i < client_count; i++) {
            if (clients[i].active && fds[i + 1].revents & POLLIN) {
                int n = read(clients[i].rfd, buffer, MAX_MSG);
                if (n > 0) {
                    buffer[n] = '\0';
                    printf("[%s]: %s", clients[i].id, buffer);
                    broadcast(buffer, clients[i].id);
                } else if (n == 0) {
                    printf("[SERVER] %s disconnected\n", clients[i].id);
                    clients[i].active = 0;
                    close(clients[i].rfd);
                    close(clients[i].wfd);
                }
            }
        }
    }

    close(reg_fd);
    unlink(reg_fifo_path);
    return 0;
}


/*

create server.sh and paste this
#!/bin/bash

# Define pipe directory (optional)
PIPE_DIR="/tmp/chat_pipes"
mkdir -p $PIPE_DIR

# Set path for registration FIFO
REG_FIFO="$PIPE_DIR/registration_fifo"

# Remove old registration FIFO
if [ -p "$REG_FIFO" ]; then
    echo "Removing old registration FIFO..."
    rm "$REG_FIFO"
fi

# Compile the server
echo "Compiling server..."
gcc -o server server.c

# Export path so the server knows where to find FIFO
export CHAT_PIPE_DIR="$PIPE_DIR"

# Run the server
echo "Starting server..."
./server

*/

