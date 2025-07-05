#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "socket.h"
#include "oscUtility.h"
#include "mediaControl.h"

#define PORT_IN 9001
#define CLIENT "127.0.0.1"
#define PORT_OUT 9000

int sockfd;
int running = 1;

void runCLI(void);

void parseArguments(int argc, char *argv[], int *inPort, char *clientIP, int *outPort, int *listenOnly) {
    *listenOnly = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--osc=", 6) == 0) {
            sscanf(argv[i] + 6, "%d:%[^:]:%d", inPort, clientIP, outPort);
        } else if (strcmp(argv[i], "--listen-only") == 0) {
            *listenOnly = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --osc=<inport>:<ip>:<outport>  Set OSC ports and IP\n");
            printf("  --listen-only                  Run in listen-only mode (no CLI)\n");
            printf("  --help                         Show this help\n");
            printf("\nDefault behavior: Start CLI with background listening\n");
            printf("\nNote: For media controls to work, you may need to:\n");
            printf("  - Run as root, or\n");
            printf("  - Add your user to the 'input' group: sudo usermod -a -G input $USER\n");
            printf("  - Then log out and back in\n");
            exit(0);
        }
    }
    
    if (*inPort == 0) *inPort = PORT_IN;
    if (clientIP[0] == '\0') strcpy(clientIP, CLIENT);
    if (*outPort == 0) *outPort = PORT_OUT;
}

void signalHandler(int sig) {
    (void)sig; 
    printf("\nShutting down...\n");
    running = 0;
    
    mediaShutdown();
    
    if (sockfd >= 0) {
        close(sockfd);
    }
    exit(0);
}

void *listenForMessages(void *arg) {
    int sockfd = *(int *)arg;
    receiveMessages(sockfd);
    return NULL;
}

int main(int argc, char *argv[]) {
    int inPort = 0, outPort = 0, listenOnly = 0;
    char clientIP[INET_ADDRSTRLEN] = {0};
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    parseArguments(argc, argv, &inPort, clientIP, &outPort, &listenOnly);
    
    loadConfig();
    
    mediaStartup();
    
    sockfd = udpSocket(inPort);
    if (sockfd < 0) {
        return EXIT_FAILURE;
    }
    
    printf("OSC Utility started - listening on port %d\n", inPort);
    
    pthread_t listenerThread;
    if (pthread_create(&listenerThread, NULL, listenForMessages, (void *)&sockfd) != 0) {
        perror("Failed to create listener thread");
        close(sockfd);
        return EXIT_FAILURE;
    }
    
    if (listenOnly) {
        printf("Running in listen-only mode. Press Ctrl+C to stop.\n");
        pthread_join(listenerThread, NULL);
    } else {
        runCLI();
    }
    
    close(sockfd);
    return EXIT_SUCCESS;
}
