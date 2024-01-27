// server.h
#ifndef SERVER_H
#define SERVER_H
#define MAX_CLIENTS 10
#define MAX_NAME_LEN 10
#define BUFFER_SIZE 256
#define TIMEOUT 40
#include <pthread.h>



typedef struct {

	int order;

	int socket_id;

	char name[MAX_NAME_LEN];

	char choice[16]; // 'rock', 'paper', or 'scissors'

	char client_response[16];

	int in_game; // 0 = not in game, 1 = in game before choice, 2 = disconnected

	pthread_t thread;

	int is_winner; // 0 = draw, 1 = win, 2 = lost

	char opponent_name[MAX_NAME_LEN];

	char opponent_choice[16];

} player;



struct arg_struct

{

	int client_index;

	int opponent_index;

} *args;

// Forward declarations for functions

void error(const char* msg);

int add_client(int socket_id);

int find_opponent(int client_index);

int determine_winner(player* player1, player* player2);

void* game_session(void* arg);

int wait_for_disconected_client(int client_index, int timeout);



player clients[MAX_CLIENTS];

int num_clients = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;



pthread_mutex_t add_player_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t player_added;



#endif // SERVER_H
