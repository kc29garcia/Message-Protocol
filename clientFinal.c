#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define CONNECT_REQUEST 1
#define CONNECT_RESPONSE 2
#define DISCONNECT_REQUEST 3
#define DISCONNECT_RESPONSE 4
#define MESSAGE_SEND 5
#define MESSAGE_ACK 6
#define SERVER_MESSAGE_SEND 7

typedef struct {
    uint8_t message_type;
    uint16_t total_length;
    uint32_t source_id;
    uint32_t destination_id;
    uint32_t sequence_number;
} ProtocolHeader;

sem_t semaphore;

uint32_t generate_id() {
    return (uint32_t)getpid();
}

//function for thread to use to receive messages from the server to display on client screen
void *receive_messages(void *arg) {
    int server_socket = *(int*)arg;
    ProtocolHeader header;
    char buffer[1024];

    while (1) {
        int bytes_received = recv(server_socket, &header, sizeof(header), 0);
        if (bytes_received <= 0) {
            break;
        }
		
		//use message type to distinguish if a message needs to be read, or if the message was received
        if (header.message_type == SERVER_MESSAGE_SEND) {
            recv(server_socket, buffer, header.total_length - sizeof(header), 0);
            printf("Message from server: %s\n", buffer);
        } else if (header.message_type == MESSAGE_ACK) {
          	printf("Message acknowledged\n");
			}
        }

    return NULL;
}

int main() {
    //create socket and connect it to server IP address. 
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    
	//CHANGE IP ADDRESS to match server IP address
	inet_pton(AF_INET, "10.176.92.15", &server_addr.sin_addr);

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
	
	//initialize semaphore and display to client that it is connected to the server 
    ProtocolHeader header;
    header.source_id = generate_id();
    header.destination_id = 0;

    sem_init(&semaphore, 0, 1);

    header.message_type = CONNECT_REQUEST;
    header.total_length = sizeof(header);

    sem_wait(&semaphore);
    send(client_socket, &header, sizeof(header), 0);
    sem_post(&semaphore);

    sem_wait(&semaphore);
    recv(client_socket, &header, sizeof(header), 0);
    sem_post(&semaphore);

    if (header.message_type == CONNECT_RESPONSE) {
        printf("Connected to server\n");
    }

	//thread creation that will handle the incomming server messages
    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receive_messages, &client_socket);
    pthread_detach(receive_thread);

    char buffer[1024];
    while (1) {
        printf("Enter message:\n");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        //if "exit" is entered then disconnect from server
		if (strcmp(buffer, "exit") == 0) {
            header.message_type = DISCONNECT_REQUEST;
            header.total_length = sizeof(header);

            sem_wait(&semaphore);
            send(client_socket, &header, sizeof(header), 0);
            sem_post(&semaphore);

            sem_wait(&semaphore);
            recv(client_socket, &header, sizeof(header), 0);
            sem_post(&semaphore);

            if (header.message_type == DISCONNECT_RESPONSE) {
                printf("Disconnected from server\n");
                break;
            }
        }

		//all other inputs will be sent to server 
        header.message_type = MESSAGE_SEND;
        header.total_length = sizeof(header) + strlen(buffer) + 1;

        sem_wait(&semaphore);
        send(client_socket, &header, sizeof(header), 0);
        send(client_socket, buffer, strlen(buffer) + 1, 0);
        sem_post(&semaphore);
    }

    close(client_socket);
    sem_destroy(&semaphore);
    return 0;
}
