#ifndef TYPES_H
#define TYPES_H

#include <pthread.h>
#include <stdbool.h>
#include <signal.h>

#define PORT 12345
#define BUFFER_SIZE 1024
#define MAX_TOTAL_CLIENTS 20
#define MAX_GAMES 20
#define MAX_WAITING_GAMES 5
#define MAX_NAME_LEN 32
#define AUTO_LIST_INTERVAL 5

typedef enum {
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_LOBBY,
    CLIENT_STATE_WAITING,
    CLIENT_STATE_PLAYING
} ClientState;

typedef enum {
    GAME_STATE_CREATED,
    GAME_STATE_WAITING,
    GAME_STATE_IN_PROGRESS,
    GAME_STATE_FINISHED
} GameState;

typedef enum {
    CELL_EMPTY = 0,
    CELL_X,
    CELL_O
} Cell;

typedef enum {
    REMATCH_CHOICE_PENDING = 0,
    REMATCH_CHOICE_YES,
    REMATCH_CHOICE_NO
} RematchChoice;

typedef struct {
    int id;
    GameState state;
    Cell board[3][3];
    int player1_fd;
    int player2_fd;
    int current_turn_fd;
    char player1_name[MAX_NAME_LEN];
    char player2_name[MAX_NAME_LEN];
    int pending_joiner_fd;
    char pending_joiner_name[MAX_NAME_LEN];
    int winner_fd;
    RematchChoice player1_accepted_rematch;
    RematchChoice player2_accepted_rematch;
    bool solo_mode;
    int moves_count;
    int owner_fd;
    bool loser_ready_for_rematch; 
} Game;

typedef struct {
    int fd;
    ClientState state;
    char name[MAX_NAME_LEN];
    int game_id;
    bool active;
    pthread_t thread_id;
} Client;

extern Game games[MAX_GAMES];
extern Client clients[MAX_TOTAL_CLIENTS];
extern int server_fd;
extern volatile sig_atomic_t keep_running;
extern int next_game_id;

extern pthread_mutex_t client_list_mutex;
extern pthread_mutex_t game_list_mutex;

#endif
