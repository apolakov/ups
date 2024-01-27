#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include "server.h"


void error(const char* msg) {
	perror(msg);
	exit(1);
}

int ping_client(int client_socket) {
    char ping_msg[] = "ping;";
    if (send(client_socket, ping_msg, strlen(ping_msg), 0) < 0) {
        perror("send ping error");
        return -1; // Error sending ping
    }

    // Set up the readfds for select and the timeout
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(client_socket, &readfds);
    timeout.tv_sec = 5;  // Wait for up to 5 seconds for a response
    timeout.tv_usec = 0;

    // Wait for a response or timeout
    int activity = select(client_socket + 1, &readfds, NULL, NULL, &timeout);
    if (activity > 0 && FD_ISSET(client_socket, &readfds)) {
        // We have a response, check for pong
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        if (recv(client_socket, buffer, sizeof(buffer), 0) > 0) {
            if (strstr(buffer, "pong") != NULL) {
                return 1; // Got pong response
            }
        }
    }
    return 0; // No pong response or error
}


int find_opponent_index_by_name(const char* opponent_name) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(clients[i].name, opponent_name) == 0) {
            return i; // Found the opponent
        }
    }
    return -1; // Opponent not found
}

void print_player(int client_index)
{
	printf("order:%d,\tsocket_id:%d,\tname:%s,\tchoice:%s,\tclient_response:%s,\tin_game:%d,\tis_winner:%d,\topponent_name:%s,\topponent_choice:%s;",
		clients[client_index].order,
		clients[client_index].socket_id,
		clients[client_index].name,
		clients[client_index].choice,
		clients[client_index].client_response,
		clients[client_index].in_game,
		clients[client_index].is_winner,
		clients[client_index].opponent_name,
		clients[client_index].opponent_choice);
	fflush(stdout);
}
void list_players()
{
	int i;
	for (i = 0; i < MAX_CLIENTS; i++)
		print_player(i);
}
// return client data as string to be send to client
char* get_client_data(int client_index, char * buffer) {
    // Example format: "choice:rock;is_winner:1;opponent_name:Alice;opponent_choice:paper"
    sprintf(buffer, "choice:%s;is_winner:%d;opponent_name:%s;opponent_choice:%s;", 
            clients[client_index].choice, 
            clients[client_index].is_winner,
            clients[client_index].opponent_name,
            clients[client_index].opponent_choice);

    printf("message: %s\n", buffer); 
    fflush(stdout);

    return buffer;
}

// Function to determine the winner of the game
int determine_winner(player* player1, player* player2) {
	if (strcmp(player1->choice, player2->choice) == 0) {
		return 0; // Draw
	}
	else if (strcmp(player1->choice, "no response") == 0)
		return 2;
	else if (strcmp(player2->choice, "no response") == 0)
		return 1;
	else if ((strcmp(player1->choice, "rock") == 0 && strcmp(player2->choice, "scissors") == 0) ||
		(strcmp(player1->choice, "scissors") == 0 && strcmp(player2->choice, "paper") == 0) ||
		(strcmp(player1->choice, "paper") == 0 && strcmp(player2->choice, "rock") == 0)) {
		return 1; // Player1 wins
	}
	else {
		return 2; // Player2 wins
	}
}

int wait_for_disconected_client(int client_index, int timeout)
{
	char* opponent_name = clients[client_index].opponent_name;
    int opponent_index = find_opponent_index_by_name(opponent_name);

    if (opponent_index == -1) {
        printf("Opponent not found.\n");
        return -1;
    }


	int attempt;
	for (attempt = 0; attempt < timeout; attempt++)
	{
			


		if (clients[client_index].in_game == 1)
		{
			printf("player to reconnect found on index %d - client %s in_game is %d\n",client_index, clients[client_index].name, clients[client_index].in_game);
			break;
		}

		if (attempt == 0) { // After the first attempt, notify the opponent
			char warning_msg[] = "info:Opponent has connection issues;";
			send(clients[opponent_index].socket_id, warning_msg, strlen(warning_msg), 0);
		}

		sleep(1); 
	}
	printf("player not connected at all\n");

	return -1;
}

void end_game(int client_index, int socked_already_closed)
{
	printf("end game for %d\n", client_index);
	clients[client_index].in_game = 0;
	clients[client_index].order = 0;
	bzero(clients[client_index].name, MAX_NAME_LEN);
	if (socked_already_closed != 1)
	{
		close(clients[client_index].socket_id);
		printf("end game for %d - socket closed\n", client_index);
	}
}

void return_client_to_loby(int client_index)
{
	printf("client %s will be returned to loby\n", clients[client_index].name);	 fflush(stdout);
	clients[client_index].in_game = 0;
	clients[client_index].is_winner = -1;
	bzero(clients[client_index].client_response, 6);
	bzero(clients[client_index].choice, 16);
	bzero(clients[client_index].opponent_choice, 6);
	pthread_mutex_trylock(&clients_mutex);
	num_clients++;
	clients[client_index].order = num_clients;
	printf("pthread_cond_signal sent\n");
	pthread_cond_signal(&player_added);
	pthread_mutex_unlock(&clients_mutex);
}

void make_result(int client_index, int opponent_index)
{
	// Determine the winner
	int winner = determine_winner(&clients[client_index], &clients[opponent_index]);
	
	// Prepare messages for each possible outcome
    char win_msg[BUFFER_SIZE];
    char lose_msg[BUFFER_SIZE];
    char draw_msg[BUFFER_SIZE];

	sprintf(win_msg, "success:result=win;choice:%s;opponent:%s;opponent_choice:%s;", 
			clients[client_index].choice, 
			clients[client_index].opponent_name, 
			clients[client_index].opponent_choice);

	sprintf(lose_msg, "success:result=lose;choice:%s;opponent:%s;opponent_choice:%s;", 
			clients[client_index].choice, 
			clients[client_index].opponent_name, 
			clients[client_index].opponent_choice);

	sprintf(draw_msg, "success:result=draw;opponent:%s;choice:%s;", 
			clients[client_index].opponent_name, 
			clients[client_index].choice);

	
	
	if (winner == 0)
	{
		send(clients[client_index].socket_id, draw_msg, strlen(draw_msg), 0);
        send(clients[opponent_index].socket_id, draw_msg, strlen(draw_msg), 0);
		clients[client_index].is_winner = 0;
		clients[opponent_index].is_winner = 0;

	}
	else if (winner == 1)
	{
		send(clients[client_index].socket_id, win_msg, strlen(win_msg), 0);
        send(clients[opponent_index].socket_id, lose_msg, strlen(lose_msg), 0);
		clients[client_index].is_winner = 1;
		clients[opponent_index].is_winner = 2;
	}
	else
	{
		send(clients[client_index].socket_id, lose_msg, strlen(lose_msg), 0);
        send(clients[opponent_index].socket_id, win_msg, strlen(win_msg), 0);
		clients[client_index].is_winner = 2;
		clients[opponent_index].is_winner = 1;
	}
	
	// send info to user
	printf("result to be sent to clients\n");
	char  msg_to_client[1024];
	get_client_data(client_index, msg_to_client);
	send(clients[client_index].socket_id, msg_to_client, strlen(msg_to_client), 0);
	char  msg_to_opponent[1024];
	get_client_data(opponent_index, msg_to_opponent);
	send(clients[opponent_index].socket_id, msg_to_opponent, strlen(msg_to_opponent), 0);

}

int fill_value_if_valid_data(int client_index, char * received_data)
{
	if (strcmp(clients[client_index].choice, "") == 0) // wait for client choice
	{
		if (strcmp(received_data, "rock") == 0
			|| strcmp(received_data, "paper") == 0
			|| strcmp(received_data, "scissors") == 0)
		{
			//response is valid
			strcpy(clients[client_index].choice, received_data);
			return 1;
		}
		else
		{
			// data are not valid
			return 0;
		}
	}
	if (clients[client_index].is_winner != -1) // wait for client response if play again
	{
		if (strcmp(received_data, "again" )== 0
			|| strcmp(received_data, "bye") == 0)
		{
			//response is valid
			strcpy(clients[client_index].client_response, received_data);
			return 1;
		}
		else
		{
			// data are not valid
			return 0;
		}
	}
	// in other cases we are not waiting for response
	return 0;
}

int do_stuff_if_needed(int client_index)
{
	// if player is about to play again, we return him to loby
	if (strcmp(clients[client_index].client_response, "again") == 0)
	{
		return_client_to_loby(client_index);
		printf("Client %d, %s is sent back in loby \n", client_index, clients[client_index].client_response); fflush(stdout);
		return 1;
	}
	return 0;
}										
void send_result(int client_index)
{
	if (clients[client_index].in_game == 1)
	{
		clients[client_index].is_winner = 1;
		strcpy(clients[client_index].opponent_choice, "opponent left");
		char  msg_to_client[1024];
		get_client_data(client_index, msg_to_client);
		send(clients[client_index].socket_id, msg_to_client, strlen(msg_to_client), 0);
		printf("result sent to client");
	}
	else
	{
		clients[client_index].is_winner = 0;
	}
}
									   
// listen for client't choice and return, if waiting should be interrupted
void* listen_for_whatever(void* arg)
{
	int client_index = *(int*)arg;
	free(arg);
	fd_set readfds;
	struct timeval timeout;
	timeout.tv_sec = TIMEOUT;
	timeout.tv_usec = 0;
	clock_t begin = clock();


	char received_data[16];
	while (1) {
		printf("new activity for %s in socket %d----------------------------\n", clients[client_index].name, clients[client_index].socket_id);
		fflush(stdout); // to see output immediatelly while parent thread waits from thread_join

		FD_ZERO(&readfds);
		FD_SET(clients[client_index].socket_id, &readfds);

		
		int activity = select(clients[client_index].socket_id + 1, &readfds, NULL, NULL, &timeout);
		if (activity < 0)
		{
			perror("select error\n");
			printf("THREAD EXIT %s-----------\n", clients[client_index].name);
			pthread_exit(0);
		}
		else if (activity == 0)
		{
			send(clients[client_index].socket_id, timeout_msg, strlen(timeout_msg), 0); // Inform client about the timeout
			printf("select timeout\n");
			printf("THREAD EXIT %s-----------\n", clients[client_index].name);
			pthread_exit(0);
		}
		else
		{
			if (FD_ISSET(clients[client_index].socket_id, &readfds)) {

				bzero(received_data, 16);
				ssize_t bytes_received = recv(clients[client_index].socket_id, received_data, sizeof(clients[client_index].choice), 0);

				if (bytes_received > 0) {
					 received_data[bytes_received] = '\0'; // Null-terminate the received string
   					 char* newline = strchr(received_data, '\n');
   					 if (newline) *newline = '\0';  // Replace newline with null character
		            
					  pthread_mutex_lock(&clients_mutex);

					// Handle "rock" command
					 if (clients[client_index].in_game == 1 && strcmp(clients[client_index].choice, "") == 0) {
						if (strcmp(received_data, "rock") == 0 || strcmp(received_data, "paper") == 0 || strcmp(received_data, "scissors") == 0) {
							strcpy(clients[client_index].choice, received_data);
							char confirmation_msg[] = "success:Choice received;";
   							send(clients[client_index].socket_id, confirmation_msg, strlen(confirmation_msg), 0);
							pthread_mutex_unlock(&clients_mutex);
							printf("Client %s chose %s\n", clients[client_index].name, received_data);
							pthread_exit(0); 
						}
						char invalid_choice_msg[] = "error:Invalid choice. Disconnecting;";
						send(clients[client_index].socket_id, invalid_choice_msg, strlen(invalid_choice_msg), 0);
						close(clients[client_index].socket_id); // Close the client's socket
						clients[client_index].in_game = 2; // Mark as disconnected
						pthread_mutex_unlock(&clients_mutex);
						pthread_exit(0); // Exit the thread as the client is disconnected

					}

					else if (clients[client_index].in_game == 0 && clients[client_index].is_winner != -1) {

						if (strcmp(received_data, "again") == 0) {
						// Handle the logic to restart the game
						return_client_to_loby(client_index);
						char again_msg[] = "success:Added back to lobby;";
    					send(clients[client_index].socket_id, again_msg, strlen(again_msg), 0);
    
						pthread_mutex_unlock(&clients_mutex);
						// Don't exit the thread; wait for a new match
						pthread_exit(0); 
						}
						else if (strcmp(received_data, "bye") == 0) {
							// Handle disconnection logic
							end_game(client_index, 0);
							char bye_msg[] = "success:Disconnected successfully;";
							send(clients[client_index].socket_id, bye_msg, strlen(bye_msg), 0);
							printf("Client %s says goodbye.\n", clients[client_index].name);
							pthread_mutex_unlock(&clients_mutex);
							pthread_exit(0); // Exit the thread as the client is leaving
						} else {

							char invalid_response_msg[] = "info:Invalid response. Disconnecting;";
							send(clients[client_index].socket_id, invalid_response_msg, strlen(invalid_response_msg), 0);
							
							close(clients[client_index].socket_id);
							end_game(client_index, 0);
							
							pthread_mutex_unlock(&clients_mutex);
							pthread_exit(0); // Exit the thread due to invalid response
						}
							// Any other invalid response
							char invalid_response_msg[] = "info:Invalid response. Disconnecting;";
							send(clients[client_index].socket_id, invalid_response_msg, strlen(invalid_response_msg), 0);
							
							close(clients[client_index].socket_id);
							end_game(client_index, 0);
							
							pthread_mutex_unlock(&clients_mutex);
							pthread_exit(0); 

					}

					
					
					 else {
						int value_is_valid = fill_value_if_valid_data(client_index, received_data);
						if (value_is_valid == 1) {
							do_stuff_if_needed(client_index);
							printf("THREAD EXIT %s-----------\n", clients[client_index].name);
                			pthread_mutex_unlock(&clients_mutex); // Unlock before exiting

							pthread_exit(0); 
						} else {
							char invalid_command_msg[] = "info:Invalid command. Disconnecting;";
							send(clients[client_index].socket_id, invalid_command_msg, strlen(invalid_command_msg), 0);
							close(clients[client_index].socket_id);
							clients[client_index].in_game = 2; // Mark the client as disconnected
							pthread_mutex_unlock(&clients_mutex);
							pthread_exit(0); // Exit the thread as the client is disconnected
						}
						pthread_mutex_unlock(&clients_mutex);
					}
				}
				else if (bytes_received == 0)
				{
					// mark client as disconnected
					clients[client_index].in_game = 2;
					printf("Server closed the connection for client %s.\n", clients[client_index].name);
					printf("listen_choice - Connection closed - client %s in game is %d, index %d \n", clients[client_index].name, clients[client_index].in_game, client_index); fflush(stdout);
					// wait for client
					clock_t end = clock();
					int remaining_time = timeout.tv_sec - (double)(end - begin) / CLOCKS_PER_SEC;
					wait_for_disconected_client(client_index, remaining_time);
					// final check
					//pthread_mutex_lock(&clients_mutex);
					if (clients[client_index].in_game == 2) // client is still dosconnected
					{
						printf("THREAD EXIT %s-----------\n", clients[client_index].name);
						pthread_exit(0);
	   
					}					
				}
				else
				{
					perror("recv error or client disconnected\n");
					printf("THREAD EXIT %s-----------\n", clients[client_index].name);
					pthread_exit(0);
				}
			}
		}
	}
	printf("THREAD EXIT %s-----------\n", clients[client_index].name);
	pthread_exit(0);
}

void* game_session(void* arg) {
	struct arg_struct* args = arg;
	int client_index = args->client_index;
	int opponent_index = args->opponent_index;
	free(arg);
	//char error_message[] = "An error occurred. The game could not be completed.\n";

	int client_order = clients[client_index].order;
	int opponent_order = clients[opponent_index].order;

	pthread_t client_thread;
	pthread_t opponent_thread;
	void* client_thread_status;
	void* opponent_thread_status;

	printf("Client %d game session started.\n", client_index);
	printf("Client %d matched with client %d.\n", client_index, opponent_index);

	// copy opponent name
	strcpy(clients[client_index].opponent_name, clients[opponent_index].name);
	strcpy(clients[opponent_index].opponent_name, clients[client_index].name);

	// Notify both clients that a match has been found.
	char  msg_to_client[1024];
	get_client_data(client_index, msg_to_client);
	send(clients[client_index].socket_id, msg_to_client, strlen(msg_to_client), 0);
	char  msg_to_opponent[1024];
	get_client_data(opponent_index, msg_to_opponent);
	send(clients[opponent_index].socket_id, msg_to_opponent, strlen(msg_to_opponent), 0);


	char timeout_msg[256];
    snprintf(timeout_msg, sizeof(timeout_msg), "info:remaining %d seconds;", TIMEOUT);
    send(clients[client_index].socket_id, timeout_msg, strlen(timeout_msg), 0);
	send(clients[opponent_index].socket_id, timeout_msg, strlen(timeout_msg), 0);

	// create threads for listening
	int* client_index2 = malloc(sizeof(int));
	*client_index2 = client_index;
	pthread_create(&client_thread, NULL, listen_for_whatever, client_index2);

	int* opponent_index2 = malloc(sizeof(int));
	*opponent_index2 = opponent_index;
	pthread_create(&opponent_thread, NULL, listen_for_whatever, opponent_index2);
	
	if (pthread_join(client_thread, &client_thread_status) != 0) {
		perror("pthread_create() join error for client\n");
	}
	if (pthread_join(opponent_thread, &opponent_thread_status) != 0) {
		perror("pthread_create() join error for opponent\n");
	}
	printf("all threads joined after choice\n");

	// set default response if client has not responded
	if (strcmp(clients[client_index].choice, "") == 0)
		strcpy(clients[client_index].choice, "no response");
	if (strcmp(clients[opponent_index].choice, "") == 0)
		strcpy(clients[opponent_index].choice, "no response");

	// copy opponent's response to client
	strcpy(clients[client_index].opponent_choice, clients[opponent_index].choice);
	strcpy(clients[opponent_index].opponent_choice, clients[client_index].choice);

	if (clients[client_index].in_game != 2 && clients[opponent_index].in_game != 2) // no player left
	{
		make_result(client_index, opponent_index);
	}
	else if (clients[client_index].in_game != 2 || clients[opponent_index].in_game != 2) // at least one player is still in game
	{
		// inform clients
		send_result(client_index);
		send_result(opponent_index);
	}

	// wait for response
	
	int client_thread_started = 0;
	int opponent_thread_started = 0;
	// create listening thread if client is not disconnected
	if (clients[client_index].in_game != 2) 
	{
		int* client_index3 = malloc(sizeof(int));
		*client_index3 = client_index;
		printf("thread for response for %s will be created\n", clients[client_index].name);
		client_thread_started = 1;
		pthread_create(&client_thread, NULL, listen_for_whatever, client_index3);
	}
	
	if (clients[opponent_index].in_game != 2)
	{
		int* opponent_index3 = malloc(sizeof(int));
		*opponent_index3 = opponent_index;
		printf("thread for response for %s will be created\n", clients[opponent_index].name);
		opponent_thread_started = 1;
		pthread_create(&opponent_thread, NULL, listen_for_whatever, opponent_index3);
	}

	// join threads which we have created
	if (client_thread_started == 1)
	{
		if (pthread_join(client_thread, &client_thread_status) != 0) {
			perror("pthread_create() join error for client\n");
		}
	}
	if (opponent_thread_started == 1)
	{
		if (pthread_join(opponent_thread, &opponent_thread_status) != 0) {
			perror("pthread_create() join error for opponent\n");
		}
	}
		
	printf("all threads joined after response\n");
	// disconnect client if he is not in other game (he did not responded "again")
	if (clients[client_index].order == client_order)
	{
		// if client is connected, inform him
		if (client_thread_started)
		{
			char busy_message[] = "info:timeout;";
			send(clients[client_index].socket_id, busy_message, strlen(busy_message), 0);
		}
		end_game(client_index, 0);
	}
	if (clients[opponent_index].order == opponent_order)
	{
		if (opponent_thread_started)
		{
			char busy_message[] = "info:timeout;";
			send(clients[opponent_index].socket_id, busy_message, strlen(busy_message), 0);
		}
		end_game(opponent_index, 0);
	}
// tu mo no potrebujem deallokova  dajak  pam 
	list_players();
	return NULL;
}

int find_opponent(int client_index) {

	printf("find_opponent\n");
	pthread_mutex_lock(&clients_mutex);
	int opponent_index = -1;
	int min_order = 1000000;// this will not work if 1000000 or players has already connected the game. Every 1000000 players server restart is required
	// we are looking for player who entered loby earlier then others
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i].order > 0){
			printf("real in_game value for %s index %d is %d, index %d\n", clients[i].name, i, clients[i].in_game, i); fflush(stdout);
			fflush(stdout);
		}
		if (clients[i].order > 0
			&& strcmp(clients[i].name, "") != 0 
			&& clients[i].in_game == 0 && i != client_index)
		{
			if (clients[i].order < min_order) 
			{
				min_order = clients[i].order;
				opponent_index = i;
				break;
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
	return opponent_index;
}

//return index of free spot in array of clients. if no free spot, return -1
int find_free_spot()
{
	int i;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].order == 0
			&& strcmp(clients[i].name, "") == 0)
			return i;
	}
	return -1;
}

// find client with given name with disconnected status
int find_client_to_reconnect(char* name)
{
	int i;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].order > 0
			&& strcmp(name, clients[i].name) == 0)
		{
			printf("player to reconnect found on index %d \n", i);
			return i;
		}
	}
	return -1;
}


// Updated function to add a client to the server
int add_client(int socket_id) {
    printf("add_client\n");

    pthread_mutex_lock(&clients_mutex);

    // Initialize new player structure
    player new_player;
    bzero(&new_player, sizeof(player)); // Zero out the structure
    new_player.socket_id = socket_id;
    new_player.in_game = 0; // Initially not in a game
    new_player.is_winner = -1;

	// Read client's name from socket
    ssize_t bytes_received = recv(socket_id, new_player.name, MAX_NAME_LEN - 1, 0);
    if (bytes_received <= 0) {
        // Handle error or disconnection
        printf("Error in receiving client name or client disconnected.\n");
        close(socket_id);
        pthread_mutex_unlock(&clients_mutex);
        return -1;
    }

    new_player.name[bytes_received] = '\0';
    char* newline = strchr(new_player.name, '\n');
    if (newline) *newline = '\0';  // Ensure null termination

	// Check if the name starts with "name:"
	if (strncmp(new_player.name, "name:", 5) == 0) {
		memmove(new_player.name, new_player.name + 5, strlen(new_player.name) - 4);
	} else {
		char problem_msg[] = "info:Opponent issues;";
		send(clients[socket_id].socket_id, problem_msg, strlen(problem_msg), 0);
		usleep(100000);
		shutdown(socket_id, SHUT_RDWR);
		close(socket_id); // Close the connection
		pthread_mutex_unlock(&clients_mutex);

		return -1; // Return an error code
	}

    printf("Received client name: %s\n", new_player.name);

    // Check for existing client with the same name
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(clients[i].name, new_player.name) == 0) {
            if (clients[i].in_game == 2) {
                // Client is reconnecting
                clients[i].socket_id = socket_id;
                clients[i].in_game = 1;
                char msg_to_client[1024];
                get_client_data(i, msg_to_client);
                send(clients[i].socket_id, msg_to_client, strlen(msg_to_client), 0);
                printf("Client [%d] with name %s is reused, new socket_id is %d", i, new_player.name, socket_id);
                pthread_mutex_unlock(&clients_mutex);
                return i;
            } else {
                // Name is in use by an active client
				char name_in_use_message[] = "error:Name already in use.;";
				send(socket_id, name_in_use_message, strlen(name_in_use_message), 0);
                shutdown(socket_id, SHUT_RDWR);
				close(socket_id);
                pthread_mutex_unlock(&clients_mutex);
                return -1;
            }
        }
    }


	//send(socket_id, "Make your move rokc/paper/scissors.\n", strlen("Hello, welcome to the game. Please tell us your name.\n"), 0);

	//usleep(100000);
    int free_spot = find_free_spot();
	
    if (free_spot == -1) {
        printf("Maximum number of clients reached. Cannot add more.\n");
        char busy_message[] = "error:ConnectionError: Server is busy. Try later;";
		
		send(socket_id, busy_message, strlen(busy_message), 0);
        close(socket_id);
        pthread_mutex_unlock(&clients_mutex);
        return -1;
    }


 	char success_msg[] = "success: logged in;";
    send(socket_id, success_msg, strlen(success_msg), 0);
    num_clients++;
    new_player.order = num_clients; // Assign order number
    clients[free_spot] = new_player;
    printf("Client [%d] is %s", free_spot, new_player.name);
    pthread_mutex_unlock(&clients_mutex);
    return free_spot;
}


void* listen_for_clients(void* arg)
{
	int sockfd = *(int*)arg;
	free(arg);
	int newsockfd;
	socklen_t clilen;
	struct sockaddr_in cli_addr;
	clilen = sizeof(cli_addr);

	while (1) {
		newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
		if (newsockfd < 0) error("ERROR on accept");
		else {
			printf("New client connected: %d\n", newsockfd);
			int* new_client_index = malloc(sizeof(int));
			*new_client_index = add_client(newsockfd);
			if (*new_client_index >= 0) {
				// Notify the client that they are waiting for an opponent.
				if (clients[*new_client_index].in_game == 0)
				{
					pthread_mutex_lock(&add_player_mutex);
					printf("lock mutex\n");
					pthread_cond_signal(&player_added); // inform main thread
					printf("signal - new player added\n");
					pthread_mutex_unlock(&add_player_mutex);
					printf("unlock mutex\n");
				}
			}
			else {
				close(newsockfd);
				free(new_client_index);
			}
		}

	}
}

int is_number(char *str) {
    // Check if the first character is a minus sign
    int start = 0;
    if (str[0] == '-') {
        // If it's just a "-", then it's not a number
        if (str[1] == '\0') {
            return 0;
        }
        start = 1; // Start checking from the next character
    }
    
    for (int i = start; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') {
            return 0;
        }
    }
    return 1;
}


int main(int argc, char* argv[]) {
	printf("Server starting...\n");
	int portno;
	struct sockaddr_in serv_addr;
	int* sockfd = malloc(sizeof(int));

	/* Initialize mutex and condition variable objects */
	pthread_mutex_init(&add_player_mutex, NULL);
	pthread_cond_init(&player_added, NULL);

	if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    if (!is_number(argv[1])) {
        fprintf(stderr, "ERROR, port must be a number\n");
        exit(1);
    }

    portno = atoi(argv[1]);
    if (portno <= 1023 || portno >= 65535) {
        fprintf(stderr, "ERROR, port must be between 1024 and 65534\n");
        exit(1);
    }

    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0) {
        error("ERROR opening socket");
    }

    bzero((char*)&serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);


	if (bind(*sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");

	listen(*sockfd, 500);

	//create thread for adding players to loby
	pthread_t listening_thread;			
	pthread_create(&listening_thread, NULL, listen_for_clients, sockfd);

	//waiting for signal
	pthread_mutex_lock(&add_player_mutex);
	while (1) {
		pthread_cond_wait(&player_added, &add_player_mutex);
		printf("thread Condition signal received.\n");

		// Find an opponent for this client.
		int client_index = find_opponent(-1);//find any player
		if (client_index >= 0)
		{
			if (ping_client(clients[client_index].socket_id) == 1) {
				int opponent_index = find_opponent(client_index);
				if (opponent_index >= 0 && ping_client(clients[opponent_index].socket_id) == 1)
				{
					clients[opponent_index].in_game = 1;
					clients[client_index].in_game = 1;

					args = malloc(sizeof(struct arg_struct) * 1);

					args->client_index = client_index;
					args->opponent_index = opponent_index;
					pthread_create(&clients[client_index].thread, NULL, game_session, args);
				}
			}
		}
	}
	pthread_mutex_unlock(&add_player_mutex);

	printf("---------------DESTROY-----------------------");
	pthread_mutex_destroy(&add_player_mutex);	
	pthread_cond_destroy(&player_added);
	close(* sockfd);
	return 0;
}