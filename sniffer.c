#include <netdb.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024
#define PORT 8080
#define PROTOCOL IPPROTO_SCTP

void sendFile();
void receiveFile(int, char *);
void serverTask(int, struct sockaddr_in);
void clientTask(char *);
void client();
void server();
void removeOccurance(char *, char);

char **servers;
int serverCount;

int main(int argc, char *argv[])
{
    int clientStatus, serverStatus;
    pid_t clientPid, serverPid;

    servers = &argv[1];
    serverCount = argc - 1;

    if ((clientPid = fork()) == 0)
    {
        client();
        exit(0);
    }

    if ((serverPid = fork()) == 0)
    {
        server();
        exit(0);
    }

    waitpid(clientPid, &clientStatus, 0);
    waitpid(serverPid, &serverStatus, 0);

    return 0;
}

void client()
{
    int n;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        n = 0;
        bzero(buffer, BUFFER_SIZE);

        while ((buffer[n++] = getchar()) != '\n')
            ;

        if ((strncmp(buffer, "dump", 4)) == 0)
        {
            // TODO: parallel execution
            for (int i = 0; i < serverCount; i++)
            {
                printf("Connecting to %s\n", servers[i]);
                clientTask(servers[i]);
            }

            continue;
        }
    }
}

void server()
{
    int socketFd, connectionFd, size;
    struct sockaddr_in serverAddress, source;

    socketFd = socket(AF_INET, SOCK_STREAM, PROTOCOL);

    if (socketFd == -1)
    {
        printf("Failed to create socket!\n");
        exit(1);
    }

    bzero(&serverAddress, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(PORT);

    if ((bind(socketFd, (struct sockaddr *)&serverAddress, sizeof(serverAddress))) != 0)
    {
        printf("Failed to bind socket!\n");
        exit(1);
    }

    while (1)
    {
        if ((listen(socketFd, 5)) != 0)
        {
            printf("Failed to listen!\n");
            exit(1);
        }

        printf("Server is listening.\n");

        size = sizeof(source);

        connectionFd = accept(socketFd, (struct sockaddr *)&source, &size);

        if (connectionFd < 0)
        {
            printf("Failed to accept connection!\n");
            exit(1);
        }

        printf("Server accepted the client.\n");

        serverTask(connectionFd, source);

        if (close(connectionFd) != 0)
        {
            printf("Failed to close connection!\n");
            exit(1);
        }
    }

    close(socketFd);
}

void clientTask(char *serverIp)
{
    int socketFd = socket(AF_INET, SOCK_STREAM, PROTOCOL);

    if (socketFd == -1)
    {
        printf("Failed to create socket!\n");
        exit(1);
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(serverIp);
    serverAddress.sin_port = htons(PORT);

    if (connect(socketFd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) != 0)
    {
        printf("Failed to connect with server!\n");
        exit(1);
    }

    receiveFile(socketFd, serverIp);

    if (close(socketFd) != 0)
    {
        printf("Failed to close socket!\n");
        exit(1);
    }
}

void serverTask(int socketFd, struct sockaddr_in source)
{
    char sourceIp[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &(source.sin_addr.s_addr), sourceIp, INET_ADDRSTRLEN);

    printf("Generating tcpdump file for %s...\n", sourceIp);

    int status;
    pid_t pid;

    if ((pid = fork()) == 0)
    {
        execlp("timeout", "timeout", "1", "tcpdump", "-i", "eth0", "-w", "dump.pcap", NULL);
        exit(0);
    }

    waitpid(pid, &status, 0);

    FILE *file = fopen("dump.pcap", "r");

    if (file == NULL)
    {
        perror("Error in reading file: dump.pcap");
        exit(1);
    }

    sendFile(file, socketFd);
}

void sendFile(FILE *file, int socketFd)
{
    printf("Sending file...\n");

    char data[BUFFER_SIZE] = {0};

    while (fgets(data, BUFFER_SIZE, file) != NULL)
    {
        if (send(socketFd, data, sizeof(data), 0) == -1)
        {
            printf("Error in sending file.");
            exit(1);
        }
        bzero(data, BUFFER_SIZE);
    }

    printf("Successfully sent the file.\n");
}

void receiveFile(int socketFd, char *serverIp)
{
    printf("Receiving file from %s...\n", serverIp);

    char cleanServerIp[20] = "";
    sprintf(cleanServerIp, "%s", serverIp);

    removeOccurance(cleanServerIp, '.');

    char fileName[30] = "";
    sprintf(fileName, "%s%s%s", "dump", cleanServerIp, ".pcap");

    FILE *file = fopen(fileName, "w");
    char buffer[BUFFER_SIZE];

    while (read(socketFd, buffer, BUFFER_SIZE) > 0)
    {
        fprintf(file, "%s", buffer);
        bzero(buffer, BUFFER_SIZE);
    }

    printf("Successfully received the file from %s.\n", serverIp);
}

void removeOccurance(char *base, char target)
{
    char *baseRead = base;
    char *baseWrite = base;

    while (*baseRead)
    {
        *baseWrite = *baseRead++;
        baseWrite += (*baseWrite != target);
    }

    *baseWrite = '\0';
}
