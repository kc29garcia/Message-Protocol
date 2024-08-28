#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define CONNECT_REQUEST 1
#define CONNECT_RESPONSE 2
#define DISCONNECT_REQUEST 3
#define DISCONNECT_RESPONSE 4
#define MESSAGE_SEND 5
#define MESSAGE_ACK 6
#define SERVER_MESSAGE_SEND 7
#define MAX_CLIENTS 100

typedef struct {
    int socket;
    int client_id;
} ClientInfo;

//client array to store multiple clients(designed to expand on my own later)
//this code can only handle one client for now
ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
sem_t semaphore;

//look up client in the array
int find_client_by_id(int client_id) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_id == client_id) {
            return clients[i].socket;
        }
    }
    return -1;
}

typedef struct {
    uint8_t message_type;
    uint16_t total_length;
    uint32_t source_id;
    uint32_t destination_id;
    uint32_t sequence_number;
} ProtocolHeader;

//function that sever thread will use to handle the client connection and messages
void *handle_client(void *arg) {
    int client_socket = *(int*)arg;
    free(arg);

    ProtocolHeader header;
    char buffer[1024];

    while (1) {
        int bytes_received = recv(client_socket, &header, sizeof(header), 0);
        if (bytes_received <= 0) {
            break;
        }

        sem_wait(&semaphore);
		
		//connect, send message, and disconnect from the client
        switch (header.message_type) {
            case CONNECT_REQUEST:
                printf("CONNECT_REQUEST received from client %d\n", header.source_id);
                clients[client_count].socket = client_socket;
                clients[client_count].client_id = header.source_id;
                client_count++;
                header.message_type = CONNECT_RESPONSE;
                send(client_socket, &header, sizeof(header), 0);
                break;
            case MESSAGE_SEND:
                recv(client_socket, buffer, header.total_length - sizeof(header), 0);
                printf("MESSAGE received from client %d: %s\n", header.source_id, buffer);
                int dest_socket = find_client_by_id(header.destination_id);
                if (dest_socket != -1) {
                    send(dest_socket, &header, sizeof(header), 0);
                    send(dest_socket, buffer, header.total_length - sizeof(header), 0);
                }
                header.message_type = MESSAGE_ACK;
                send(client_socket, &header, sizeof(header), 0);
                break;
            case DISCONNECT_REQUEST:
                printf("DISCONNECT_REQUEST received from client %d\n", header.source_id);
                header.message_type = DISCONNECT_RESPONSE;
                send(client_socket, &header, sizeof(header), 0);
                close(client_socket);
                sem_post(&semaphore);
                return NULL;
        }

        sem_post(&semaphore);
    }

    close(client_socket);
    return NULL;
}

//function used for server thread to take input and send to client
void *server_user_input(void *arg) {
    while (1) {
        char buffer[1024];
        printf("Enter message (format for input: <client_id> <message>): \n");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        uint32_t client_id;
        char message[1024];

        if (sscanf(buffer, "%u %[^\n]", &client_id, message) != 2) {
            printf("INVALID INPUT. Use: <client ID> <message>\n");
            continue;
        }

        int client_socket = find_client_by_id(client_id);
        if (client_socket == -1) {
            printf("Client ID not found.\n");
            continue;
        }

        ProtocolHeader header;
        header.message_type = SERVER_MESSAGE_SEND;
        header.total_length = sizeof(header) + strlen(message) + 1;
        header.source_id = 0;
        header.destination_id = client_id;

        sem_wait(&semaphore);
        send(client_socket, &header, sizeof(header), 0);
        send(client_socket, message, strlen(message) + 1, 0);
        sem_post(&semaphore);

        printf("Message sent to client %u\n", client_id);
    }
}

int main() {
    //create socket
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation fail");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failure");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
	
	//listen for clients to connect to port
    if (listen(server_socket, 5) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port 8080...\n");

    sem_init(&semaphore, 0, 1);

	//thread creation to handle input on server end
    pthread_t input_thread;
    pthread_create(&input_thread, NULL, server_user_input, NULL);
    pthread_detach(input_thread);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }

		//create new thread for the client and call the function to handle client
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_socket;
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, (void*)new_sock);
        pthread_detach(thread);
    }

    close(server_socket);
    sem_destroy(&semaphore);
    return 0;
}
