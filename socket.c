#include "socket.h"
#include "oscUtility.h"

int udpSocket(int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setting socket options failed");
        close(sockfd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

void receiveMessages(int sockfd) {
    char buffer[1024];
    struct sockaddr_in srcAddr;
    socklen_t addrLen = sizeof(srcAddr);

    while (1) {
        ssize_t bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                       (struct sockaddr *)&srcAddr, &addrLen);
        if (bytesReceived < 0) {
            perror("Receive failed");
            continue;
        }

        buffer[bytesReceived] = '\0';
        
        if (isMessagePrintingEnabled()) {
            printf("Received message: %s\n", buffer);

            char srcIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &srcAddr.sin_addr, srcIP, sizeof(srcIP));
            printf("From IP: %s, Port: %d\n", srcIP, ntohs(srcAddr.sin_port));
        }
        
        checkParameterFilter(buffer);
    }
}
