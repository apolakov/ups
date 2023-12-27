#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_CLIENTS 10
#define MAX_NAME_LEN 50
#define BUFFER_SIZE 256

typedef struct {
    int socket_id;
    char name[MAX_NAME_LEN];
    char choice[16]; // 'rock', 'paper', or 'scissors'
    int in_game;
    pthread_t thread;
} player;

// Forward declarations for functions
void error(const char *msg);
int add_client(int socket_id);
int find_opponent(int client_index);
int determine_winner(player *player1, player *player2);
void *game_session(void *arg);
void mark_client_ready(int client_index);
void enqueue_client(int client_index);
void dequeue_client(int client_index);
void add_waiting_client(int client_index);
void remove_waiting_client(int client_index);
void initialize_waiting_clients(void);

// Queue to hold clients that are ready to play
int ready_queue[MAX_CLIENTS];
int queue_size = 0;

// Initialize the waiting_clients array and mutex
bool waiting_clients[MAX_CLIENTS];
pthread_mutex_t waiting_clients_mutex = PTHREAD_MUTEX_INITIALIZER;




player clients[MAX_CLIENTS];
int num_clients = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void error(const char *msg) {
    perror(msg);
    exit(1);
}


// Function to determine the winner of the game
int determine_winner(player *player1, player *player2) {
    if (strcmp(player1->choice, player2->choice) == 0) {
        return 0; // Draw
    } else if ((strcmp(player1->choice, "rock") == 0 && strcmp(player2->choice, "scissors") == 0) ||
               (strcmp(player1->choice, "scissors") == 0 && strcmp(player2->choice, "paper") == 0) ||
               (strcmp(player1->choice, "paper") == 0 && strcmp(player2->choice, "rock") == 0)) {
        return 1; // Player1 wins
    } else {
        return 2; // Player2 wins
    }
}

void *game_session(void *arg) {
    int client_index = *(int*)arg;
    free(arg);
    char error_message[] = "An error occurred. The game could not be completed.\n";

    printf("Client %d game session started.\n", client_index);

    // Notify the client that they are waiting for an opponent.
    char wait_message[] = "Waiting for an opponent...\n";
    send(clients[client_index].socket_id, wait_message, strlen(wait_message), 0);
    
    // Find an opponent for this client.
    int opponent_index = find_opponent(client_index);
    while (opponent_index == -1) {
        sleep(1); // Avoid busy waiting.
        opponent_index = find_opponent(client_index);
    }

    printf("Client %d matched with client %d.\n", client_index, opponent_index);

    // Notify both clients that a match has been found.
    char msg_to_first_client[1024];
    char msg_to_second_client[1024];
    snprintf(msg_to_first_client, sizeof(msg_to_first_client), "Match found with %s! Please make your move.\n", clients[opponent_index].name);
    snprintf(msg_to_second_client, sizeof(msg_to_second_client), "Match found with %s! Please make your move.\n", clients[client_index].name);
    send(clients[client_index].socket_id, msg_to_first_client, strlen(msg_to_first_client), 0);
    send(clients[opponent_index].socket_id, msg_to_second_client, strlen(msg_to_second_client), 0);

    // Initialize the select() parameters.
    fd_set readfds;
    int max_sd = clients[client_index].socket_id > clients[opponent_index].socket_id ? clients[client_index].socket_id : clients[opponent_index].socket_id;

    struct timeval timeout;
    timeout.tv_sec = 60; // 60-second timeout for the match
    timeout.tv_usec = 0;

    // Initialize the choices.
    char client_choices[2][16] = {{'\0'}, {'\0'}};
    int clients_ready = 0;

    while (clients_ready < 2) {
        FD_ZERO(&readfds);
        FD_SET(clients[client_index].socket_id, &readfds);
        FD_SET(clients[opponent_index].socket_id, &readfds);

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            perror("select error");
            break;
        } else if (activity == 0) {
            printf("select timeout\n");
            break;
        } else {
            if (FD_ISSET(clients[client_index].socket_id, &readfds)) {
                ssize_t bytes_received = recv(clients[client_index].socket_id, clients[client_index].choice, sizeof(clients[client_index].choice) - 1, 0);
                if (bytes_received > 0) {
                    clients[client_index].choice[bytes_received] = '\0';
                    printf("Client %d chose: %s\n", client_index, clients[client_index].choice);
                    clients_ready++;
                } else {
                    perror("recv error or client disconnected");
                    break;
                }
            }

            if (FD_ISSET(clients[opponent_index].socket_id, &readfds)) {
                ssize_t bytes_received = recv(clients[opponent_index].socket_id, clients[opponent_index].choice, sizeof(clients[opponent_index].choice) - 1, 0);
                if (bytes_received > 0) {
                    clients[opponent_index].choice[bytes_received] = '\0';
                    printf("Client %d chose: %s\n", opponent_index, clients[opponent_index].choice);
                    clients_ready++;
                } else {
                    perror("recv error or opponent disconnected");
                    break;
                }
            }
        }
    }

    char result_message[1024];
    // Determine the winner and send the results.
    if (clients_ready == 2) {
        int winner = determine_winner(&clients[client_index], &clients[opponent_index]);


        char move_announcement[1024];
        sprintf(move_announcement, "Your move is %s. Your opponent's move is %s.\n",
            clients[client_index].choice, clients[opponent_index].choice);
        send(clients[client_index].socket_id, move_announcement, strlen(move_announcement), 0);
        send(clients[opponent_index].socket_id, move_announcement, strlen(move_announcement), 0);



        char result_message[1024];

        if (winner == 0) {
            sprintf(result_message, "It's a draw!\n");
        } else if (winner == 1) {
            sprintf(result_message, "%s wins!\n", clients[client_index].name);
        } else {
            sprintf(result_message, "%s wins!\n", clients[opponent_index].name);
        }
        printf("Game result: %s\n", result_message);

       send(clients[client_index].socket_id, result_message, strlen(result_message), 0);
        send(clients[opponent_index].socket_id, result_message, strlen(result_message), 0);
    } else {
        // Handle the case where not all clients have made a move.
        char error_message[] = "An error occurred. The game could not be completed.\n";
        send(clients[client_index].socket_id, error_message, strlen(error_message), 0);
        send(clients[opponent_index].socket_id, error_message, strlen(error_message), 0);
    }

    // Reset the in-game state and close the sockets.
    pthread_mutex_lock(&clients_mutex);
    clients[client_index].in_game = 0;
    clients[opponent_index].in_game = 0;
    close(clients[client_index].socket_id);
    close(clients[opponent_index].socket_id);
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}



int main(int argc, char *argv[]) {
    printf("Server starting...\n");
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    initialize_waiting_clients();
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR on accept");
        else{
            printf("New client connected: %d\n", newsockfd);
            int *new_client_index = malloc(sizeof(int));
            *new_client_index = add_client(newsockfd);
            if (*new_client_index >= 0) {
                pthread_create(&clients[*new_client_index].thread, NULL, game_session, new_client_index);
            } else {
                close(newsockfd);
                free(new_client_index);
            }
        }

    }

    close(sockfd);
    return 0;
}

// Function to mark a client as ready and looking for a game
void mark_client_ready(int client_index) {
    pthread_mutex_lock(&clients_mutex);
    clients[client_index].in_game = 1; // Use this flag to indicate readiness to play
    pthread_mutex_unlock(&clients_mutex);
}

// Enqueue a client
void enqueue_client(int client_index) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ready_queue[i] == -1) {  // Assume -1 indicates an empty slot
            ready_queue[i] = client_index;
            clients[client_index].in_game = 1; // Mark as ready
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Dequeue a client
void dequeue_client(int client_index) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ready_queue[i] == client_index) {
            ready_queue[i] = -1; // Mark as empty
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}


int find_opponent(int client_index) {
    pthread_mutex_lock(&clients_mutex);
    int opponent_index = -1;
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].in_game == 0 && i != client_index) {
            opponent_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return opponent_index;
}

void initialize_waiting_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        waiting_clients[i] = false;
    }
}

// Updated function to add a client to the server
int add_client(int socket_id) {
        pthread_mutex_lock(&clients_mutex);
    if (num_clients >= MAX_CLIENTS) {
        printf("Maximum number of clients reached. Cannot add more.\n");
        pthread_mutex_unlock(&clients_mutex);
        return -1;
    }

    player new_player;
    new_player.socket_id = socket_id;
    new_player.in_game = 0; // Initially not in a game
    bzero(new_player.name, MAX_NAME_LEN);
    bzero(new_player.choice, 16);


    // Read client's name from socket
    recv(socket_id, new_player.name, MAX_NAME_LEN - 1, 0);
    new_player.name[MAX_NAME_LEN - 1] = '\0'; // Ensure null termination

    clients[num_clients] = new_player;
    int index = num_clients;
    num_clients++;
    waiting_clients[index] = true; // Mark the client as waiting

    pthread_mutex_unlock(&clients_mutex);
    return index;
}

// Function to remove a client from the list of waiting clients
void remove_waiting_client(int client_index) {
    pthread_mutex_lock(&waiting_clients_mutex);
    waiting_clients[client_index] = false;
    pthread_mutex_unlock(&waiting_clients_mutex);
}