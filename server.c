#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "server.h"
#include "gpio.h"
#include "parser.h"
#include "utils.h"

int start_server(int pi, int port) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    unsigned char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    logger("Server listening on port %d\n", port);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("recv");
        } else {
            logger("Received: %d bytes.\n", bytes_received);
            send(client_fd, "Message received", 17, 0);
            struct parse_result result = parse_message(buffer);
            if(result.result == 0) {
                logger("Successfully parsed and checked packet, processing.");
                set_color(pi, result.RED, result.BLUE, result.GREEN);
            }
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
