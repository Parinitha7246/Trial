#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <poll.h>
#include <errno.h>

#define MAX_CLIENTS 10
#define MAX_NAME 50
#define MAX_MSG 256

struct Client {
    char id[MAX_NAME];
    int rfd; // Read from client
    int wfd; // Write to client
    int active;
};

struct Client clients[MAX_CLIENTS];
int client_count = 0;

void create_fifo(const char *name) {
    unlink(name); // Remove if it exists
    if (mkfifo(name, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
}

void broadcast(const char *msg, const char *sender_id) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].active && strcmp(clients[i].id, sender_id) != 0) {
            write(clients[i].wfd, msg, strlen(msg));
        }
    }
}

int find_client_index_by_fd(int fd) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].rfd == fd && clients[i].active) {
            return i;
        }
    }
    return -1;
}

int main() {
    create_fifo("registration_fifo");
    int reg_fd = open("registration_fifo", O_RDONLY | O_NONBLOCK);
    if (reg_fd < 0) {
        perror("Failed to open registration_fifo");
        exit(EXIT_FAILURE);
    }

    struct pollfd fds[MAX_CLIENTS + 1];
    fds[0].fd = reg_fd;
    fds[0].events = POLLIN;

    printf("[SERVER] Started. Waiting for clients...\n");

    char buffer[MAX_MSG];

    while (1) {
        int nfds = 1;
        for (int i = 0; i < client_count; i++) {
            if (clients[i].active) {
                fds[nfds].fd = clients[i].rfd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        int ret = poll(fds, nfds, 1000);
        if (ret < 0) {
            perror("poll");
            continue;
        }

        if (fds[0].revents & POLLIN) {
            char client_id[MAX_NAME];
            int bytes = read(reg_fd, client_id, MAX_NAME);
            if (bytes > 0) {
                client_id[bytes] = '\0';
                printf("[SERVER] Registration from: %s\n", client_id);

                if (client_count >= MAX_CLIENTS) {
                    printf("[SERVER] Max clients reached. Ignoring %s\n", client_id);
                    continue;
                }

                char to_server[100], to_client[100];
                snprintf(to_server, sizeof(to_server), "%s_to_server", client_id);
                snprintf(to_client, sizeof(to_client), "server_to_%s", client_id);

                create_fifo(to_server);
                create_fifo(to_client);

                int rfd = open(to_server, O_RDONLY | O_NONBLOCK);
                int wfd = open(to_client, O_WRONLY);

                if (rfd < 0 || wfd < 0) {
                    perror("open client FIFO");
                    continue;
                }

                write(wfd, "READY\n", 6);

                strcpy(clients[client_count].id, client_id);
                clients[client_count].rfd = rfd;
                clients[client_count].wfd = wfd;
                clients[client_count].active = 1;
                client_count++;
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                int idx = find_client_index_by_fd(fds[i].fd);
                if (idx == -1) continue;

                int n = read(clients[idx].rfd, buffer, MAX_MSG);
                if (n > 0) {
                    buffer[n] = '\0';
                    printf("[%s]: %s", clients[idx].id, buffer);
                    broadcast(buffer, clients[idx].id);
                } else if (n == 0) {
                    printf("[SERVER] %s disconnected\n", clients[idx].id);
                    clients[idx].active = 0;
                    close(clients[idx].rfd);
                    close(clients[idx].wfd);
                }
            }
        }
    }

    close(reg_fd);
    unlink("registration_fifo");
    return 0;
}
