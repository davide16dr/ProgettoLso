#include "protocol.h"
#include "utils.h"
#include "game_logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Costanti messaggi 
const char *CMD_NAME_PREFIX = "NAME ";
const char *CMD_LIST = "LIST";
const char *CMD_CREATE = "CREATE";
const char *CMD_JOIN_REQUEST_PREFIX = "JOIN_REQUEST ";
const char *CMD_ACCEPT_PREFIX = "ACCEPT ";
const char *CMD_REJECT_PREFIX = "REJECT ";
const char *CMD_MOVE_PREFIX = "MOVE ";
const char *CMD_QUIT = "QUIT";
const char *CMD_REMATCH_YES = "REMATCH YES";
const char *CMD_REMATCH_NO = "REMATCH NO";
const char *CMD_REMATCH_JOIN_PREFIX = "REMATCH_JOIN ";
const char *CMD_ACK_GAMEOVER = "ACK_GAMEOVER";
const char *CMD_ACK_GAMEOVER_LOSER = "ACK_GAMEOVER_LOSER";
const char *CMD_GET_NAME = "CMD:GET_NAME\n";
const char *CMD_REMATCH_OFFER = "CMD:REMATCH_OFFER\n";
const char *RESP_NAME_OK = "RESP:NAME_OK\n";
const char *RESP_CREATED_FMT = "RESP:CREATED %d\n";
const char *RESP_GAMES_LIST_PREFIX = "RESP:GAMES_LIST;";
const char *RESP_REQUEST_SENT_FMT = "RESP:REQUEST_SENT %d\n";
const char *RESP_JOIN_ACCEPTED_FMT = "RESP:JOIN_ACCEPTED %d %c %s\n";
const char *RESP_REJECT_OK_FMT = "RESP:REJECT_OK %s\n";
const char *RESP_JOIN_REJECTED_FMT = "RESP:JOIN_REJECTED %d %s\n";
const char *RESP_QUIT_OK = "RESP:QUIT_OK Tornare alla lobby.\n";
const char *RESP_REMATCH_ACCEPTED_FMT = "RESP:REMATCH_ACCEPTED %d In attesa di un nuovo avversario.\n";
const char *RESP_REMATCH_DECLINED = "RESP:REMATCH_DECLINED Tornare alla lobby.\n";
const char *NOTIFY_JOIN_REQUEST_FMT = "NOTIFY:JOIN_REQUEST %s\n";
const char *NOTIFY_GAME_START_FMT = "NOTIFY:GAME_START %d %c %s\n";
const char *NOTIFY_REQUEST_CANCELLED_FMT = "NOTIFY:REQUEST_CANCELLED %s se n'è andato\n";
const char *NOTIFY_OPPONENT_ACCEPTED_REMATCH = "NOTIFY:OPPONENT_ACCEPTED_REMATCH %d\n";
const char *NOTIFY_OPPONENT_DECLINED = "NOTIFY:OPPONENT_DECLINED Tornare alla lobby.\n";
const char *ERR_NAME_TAKEN = "ERROR:NAME_TAKEN\n";
const char *ERR_SERVER_FULL_GAMES = "ERROR:Server pieno, impossibile creare una partita (nessuno slot disponibile)\n";
const char *ERR_SERVER_FULL_SLOTS = "ERROR:Il server è pieno. Riprova più tardi.\n";
const char *ERR_INVALID_MOVE_FORMAT = "ERROR:Formato della mossa non valido. Usa: MOVE <riga> <colonna>\n";
const char *ERR_INVALID_MOVE_BOUNDS = "ERROR:Mossa non valida (fuori dai limiti 0-2)\n";
const char *ERR_INVALID_MOVE_OCCUPIED = "ERROR:Mossa non valida (cella occupata)\n";
const char *ERR_NOT_YOUR_TURN = "ERROR:Non è il tuo turno\n";
const char *ERR_GAME_NOT_FOUND = "ERROR:Partita non trovata\n";
const char *ERR_GAME_NOT_IN_PROGRESS = "ERROR:Partita non in corso\n";
const char *ERR_GAME_NOT_WAITING = "ERROR:La partita non è in attesa di giocatori\n";
const char *ERR_GAME_ALREADY_STARTED = "ERROR:Partita già iniziata\n";
const char *ERR_GAME_FINISHED = "ERROR:Partita terminata\n";
const char *ERR_CANNOT_JOIN_OWN_GAME = "ERROR:Non puoi unirti alla tua partita\n";
const char *ERR_ALREADY_PENDING = "ERROR:Il creatore della partita è occupato con un'altra richiesta di adesione\n";
const char *ERR_NO_PENDING_REQUEST = "ERROR:Nessuna richiesta di adesione in sospeso trovata per questo giocatore\n";
const char *ERR_JOINER_LEFT = "ERROR:Il giocatore che ha richiesto di unirsi non è più disponibile.\n";
const char *ERR_CREATOR_LEFT = "ERROR:Il creatore della partita sembra disconnesso.\n";
const char *ERR_NOT_IN_LOBBY = "ERROR:Comando disponibile solo nello stato LOBBY\n";
const char *ERR_NOT_WAITING = "ERROR:Comando disponibile solo nello stato WAITING\n";
const char *ERR_NOT_PLAYING = "ERROR:Comando disponibile solo nello stato PLAYING o a partita terminata\n";
const char *ERR_UNKNOWN_COMMAND_FMT = "ERROR:Comando sconosciuto o stato non valido (%d) per il comando: %s\n";
const char *ERR_NOT_FINISHED_GAME = "ERROR:Comando disponibile solo dopo che la partita è terminata\n";
const char *ERR_NOT_THE_WINNER = "ERROR:Solo il vincitore può decidere il rematch\n";
const char *ERR_NOT_IN_FINISHED_OR_DRAW_GAME = "ERROR:Comando rematch non valido nello stato attuale della partita\n";
const char *ERR_INVALID_REMATCH_CHOICE = "ERROR:Scelta del comando rematch non valido\n";
const char *ERR_DRAW_REMATCH_ONLY_PLAYER = "ERROR:Impossibile richiedere il rematch dopo un pareggio se non si è un giocatore nella partita\n";
const char *ERR_GENERIC = "ERROR:Si è verificato un errore interno del server.\n";

void broadcast_games_list() {
    char response[BUFFER_SIZE * 2];
    strcpy(response, RESP_GAMES_LIST_PREFIX);
    bool first_game = true;
    
    pthread_mutex_lock(&game_list_mutex);
    for (int i = 0; i < MAX_GAMES; ++i) {
        if (games[i].state == GAME_STATE_EMPTY || games[i].state == GAME_STATE_FINISHED) {
            continue;
        }
        
        if (!first_game) strncat(response, "|", sizeof(response) - strlen(response) - 1);
        
        char game_info[200];
        
        if (games[i].state == GAME_STATE_WAITING) {
            // Partita in attesa
            if (games[i].player2_fd == -2) {
                // Avversario disconnesso
                snprintf(game_info, sizeof(game_info), "%d,%s,Waiting-Disconnected,%s", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?",
                         games[i].player2_name[0] ? games[i].player2_name : "");
            } else {
                // Nessun avversario
                snprintf(game_info, sizeof(game_info), "%d,%s,Waiting", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?");
            }
        }
        else if (games[i].state == GAME_STATE_IN_PROGRESS) {
            // Partita in corso
            if (games[i].player2_fd >= 0 && games[i].player2_name[0]) {
                // Avversario presente
                snprintf(game_info, sizeof(game_info), "%d,%s,In Progress,%s", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?",
                         games[i].player2_name);
            } else if (games[i].player2_fd == -2 && games[i].player2_name[0]) {
                // Avversario disconnesso
                snprintf(game_info, sizeof(game_info), "%d,%s,In Progress-Paused,%s", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?",
                         games[i].player2_name);
            } else {
                // Caso anomalo: in progress senza player2
                snprintf(game_info, sizeof(game_info), "%d,%s,In Progress", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?");
            }
        }
        else {
            // Stato sconosciuto o altri stati
            snprintf(game_info, sizeof(game_info), "%d,%s,Unknown", 
                     games[i].id, 
                     games[i].player1_name[0] ? games[i].player1_name : "?");
        }
        
        strncat(response, game_info, sizeof(response) - strlen(response) - 1);
        first_game = false;
    }
    pthread_mutex_unlock(&game_list_mutex);
    
    strncat(response, "\n", sizeof(response) - strlen(response) - 1);
    
    // Invia a tutti i client in LOBBY
    pthread_mutex_lock(&client_list_mutex);
    for (int i = 0; i < MAX_TOTAL_CLIENTS; i++) {
        if (clients[i].active && clients[i].state == CLIENT_STATE_LOBBY) {
            send_to_client(clients[i].fd, response);
        }
    }
    pthread_mutex_unlock(&client_list_mutex);
}
void process_name_command(int client_idx, const char *name_arg) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS || !name_arg)
        return;
    
    int fd = -1;
    ClientState current_state;
    bool name_taken = false;

    pthread_mutex_lock(&client_list_mutex);

    if (!clients[client_idx].active || clients[client_idx].state != CLIENT_STATE_CONNECTED) {
        fd = clients[client_idx].active ? clients[client_idx].fd : -1;
        current_state = clients[client_idx].active ? clients[client_idx].state : CLIENT_STATE_CONNECTED;
        pthread_mutex_unlock(&client_list_mutex);
        LOG("Client fd %d (idx %d) ha inviato NAME in stato errato (%d) o è inattivo.\n", fd, client_idx, current_state);
        return;
    }
    fd = clients[client_idx].fd;

    char clean_name[MAX_NAME_LEN];
    strncpy(clean_name, name_arg, MAX_NAME_LEN - 1);
    clean_name[MAX_NAME_LEN - 1] = '\0';
    clean_name[strcspn(clean_name, "\r\n ")] = 0;

    if (strlen(clean_name) == 0) {
        pthread_mutex_unlock(&client_list_mutex);
        LOG("Client fd %d (idx %d) ha tentato di registrare un nome vuoto.\n", fd, client_idx);
        send_to_client(fd, "ERROR:Name cannot be empty.\n");
        return;
    }

    for (int i = 0; i < MAX_TOTAL_CLIENTS; ++i) {
        if (i != client_idx && clients[i].active && clients[i].name[0] != '\0') {
            if (strcmp(clients[i].name, clean_name) == 0) {
                name_taken = true;
                break;
            }
        }
    }

    if (name_taken) {
        pthread_mutex_unlock(&client_list_mutex);
        LOG("Client fd %d (idx %d) ha tentato un nome duplicato: %s\n", fd, client_idx, clean_name);
        send_to_client(fd, ERR_NAME_TAKEN);
    } else {
        strncpy(clients[client_idx].name, clean_name, MAX_NAME_LEN - 1);
        clients[client_idx].name[MAX_NAME_LEN - 1] = '\0';
        clients[client_idx].state = CLIENT_STATE_LOBBY;
        LOG("Client fd %d (idx %d) ha registrato il nome: %s\n", fd, client_idx, clients[client_idx].name);

        pthread_mutex_unlock(&client_list_mutex);

        char name_response[BUFFER_SIZE];
        snprintf(name_response, sizeof(name_response), RESP_NAME_OK, clients[client_idx].name);
        send_to_client(fd, name_response);
        
        // NUOVO: Invia lista aggiornata
        broadcast_games_list();
    }
}

void process_list_command(int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS)
        return;
    
    char response[BUFFER_SIZE * 2];
    response[0] = '\0';
    strcat(response, RESP_GAMES_LIST_PREFIX);
    bool first_game = true;
    int client_fd = -1;
    
    pthread_mutex_lock(&client_list_mutex);
    if (!clients[client_idx].active) {
        pthread_mutex_unlock(&client_list_mutex);
        return;
    }
    client_fd = clients[client_idx].fd;
    pthread_mutex_unlock(&client_list_mutex);

    pthread_mutex_lock(&game_list_mutex);
    for (int i = 0; i < MAX_GAMES; ++i) {
        if (games[i].state == GAME_STATE_EMPTY || games[i].state == GAME_STATE_FINISHED) {
            continue;
        }
        
        if (!first_game)
            strncat(response, "|", sizeof(response) - strlen(response) - 1);
        
        char game_info[200];
        
        if (games[i].state == GAME_STATE_WAITING) {
            if (games[i].player2_fd == -2) {
                snprintf(game_info, sizeof(game_info), "%d,%s,Waiting-Disconnected,%s", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?",
                         games[i].player2_name[0] ? games[i].player2_name : "");
            } else {
                snprintf(game_info, sizeof(game_info), "%d,%s,Waiting", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?");
            }
        }
        else if (games[i].state == GAME_STATE_IN_PROGRESS) {
            if (games[i].player2_fd >= 0 && games[i].player2_name[0]) {
                snprintf(game_info, sizeof(game_info), "%d,%s,In Progress,%s", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?",
                         games[i].player2_name);
            } else if (games[i].player2_fd == -2 && games[i].player2_name[0]) {
                snprintf(game_info, sizeof(game_info), "%d,%s,In Progress-Paused,%s", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?",
                         games[i].player2_name);
            } else {
                snprintf(game_info, sizeof(game_info), "%d,%s,In Progress", 
                         games[i].id, 
                         games[i].player1_name[0] ? games[i].player1_name : "?");
            }
        }
        else {
            snprintf(game_info, sizeof(game_info), "%d,%s,Unknown", 
                     games[i].id, 
                     games[i].player1_name[0] ? games[i].player1_name : "?");
        }
        
        strncat(response, game_info, sizeof(response) - strlen(response) - 1);
        first_game = false;
    }
    pthread_mutex_unlock(&game_list_mutex);
    
    strncat(response, "\n", sizeof(response) - strlen(response) - 1);
    send_to_client(client_fd, response);
}
void process_create_command(int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS)
        return;
    
    char response[BUFFER_SIZE];
    int game_idx = -1;
    int created_game_id = -1;
    int client_fd = -1;
    
    pthread_mutex_lock(&client_list_mutex);
    pthread_mutex_lock(&game_list_mutex);
    
    if (!clients[client_idx].active || clients[client_idx].state != CLIENT_STATE_LOBBY) {
        client_fd = clients[client_idx].active ? clients[client_idx].fd : -1;
        ClientState state = clients[client_idx].active ? clients[client_idx].state : CLIENT_STATE_LOBBY;
        LOG("Client fd %d (idx %d) ha inviato CREATE in stato errato (%d).\n", client_fd, client_idx, state);
        snprintf(response, sizeof(response), "%s\n", ERR_NOT_IN_LOBBY);
        goto create_cleanup;
    }
    client_fd = clients[client_idx].fd;
    
    for (int i = 0; i < MAX_GAMES; ++i) {
        if (games[i].state == GAME_STATE_EMPTY) {
            game_idx = i;
            break;
        }
    }
    
    if (game_idx != -1) {
        LOG("Uso slot partita vuoto %d per nuova partita.\n", game_idx);
        created_game_id = next_game_id++;
        games[game_idx].id = created_game_id;
        games[game_idx].state = GAME_STATE_WAITING;
        init_board(games[game_idx].board);
        games[game_idx].player1_fd = client_fd;
        games[game_idx].player2_fd = -1;
        games[game_idx].current_turn_fd = client_fd;
        games[game_idx].winner_fd = -1;
        games[game_idx].player1_accepted_rematch = REMATCH_CHOICE_PENDING;
        games[game_idx].player2_accepted_rematch = REMATCH_CHOICE_PENDING;
        games[game_idx].moves_count = 0;
        games[game_idx].owner_fd = client_fd; 
        strncpy(games[game_idx].player1_name, clients[client_idx].name, MAX_NAME_LEN);
        games[game_idx].player1_name[MAX_NAME_LEN - 1] = '\0';
        games[game_idx].player2_name[0] = '\0';
        games[game_idx].pending_joiner_fd = -1;
        games[game_idx].pending_joiner_name[0] = '\0';
        
        clients[client_idx].state = CLIENT_STATE_LOBBY;
        clients[client_idx].game_id = created_game_id;
        
        snprintf(response, sizeof(response), RESP_CREATED_FMT, created_game_id);
        LOG("Partita %d creata da %s (fd %d) nello slot %d. Owner_fd impostato.\n", 
            created_game_id, games[game_idx].player1_name, client_fd, game_idx);
    } else {
        snprintf(response, sizeof(response), "%s\n", ERR_SERVER_FULL_GAMES);
        LOG("Creazione partita fallita per %s (fd %d): Nessuno slot partita vuoto.\n", clients[client_idx].name, client_fd);
    }
    
create_cleanup:
    pthread_mutex_unlock(&game_list_mutex);
    pthread_mutex_unlock(&client_list_mutex);
    
    if (client_fd >= 0) {
        send_to_client(client_fd, response);
        
        if (game_idx != -1) {
            broadcast_games_list();
        }
    }
}

void process_join_request_command(int client_idx, const char *game_id_str) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS || !game_id_str)
        return;
    
    int game_id_to_join = atoi(game_id_str);
    int game_idx = -1;
    int joiner_fd = -1;
    char joiner_name[MAX_NAME_LEN] = {0};
    char response_joiner[BUFFER_SIZE];
    char notify_creator[BUFFER_SIZE];
    bool request_sent = false;
    
    pthread_mutex_lock(&client_list_mutex);
    pthread_mutex_lock(&game_list_mutex);
    
    if (!clients[client_idx].active) {
        joiner_fd = clients[client_idx].active ? clients[client_idx].fd : -1;
        snprintf(response_joiner, sizeof(response_joiner), "ERROR:Client non attivo\n");
        goto join_cleanup;
    }
    
    if (clients[client_idx].state != CLIENT_STATE_LOBBY && 
        clients[client_idx].state != CLIENT_STATE_WAITING) {
        joiner_fd = clients[client_idx].active ? clients[client_idx].fd : -1;
        snprintf(response_joiner, sizeof(response_joiner), "%s\n", ERR_NOT_IN_LOBBY);
        goto join_cleanup;
    }
    
    joiner_fd = clients[client_idx].fd;
    strncpy(joiner_name, clients[client_idx].name, MAX_NAME_LEN - 1);
    joiner_name[MAX_NAME_LEN - 1] = '\0';
    
    if (clients[client_idx].state == CLIENT_STATE_WAITING && 
        clients[client_idx].game_id > 0) {
        int old_game_idx = find_game_index_unsafe(clients[client_idx].game_id);
        if (old_game_idx != -1) {
            Game *old_game = &games[old_game_idx];
            if (old_game->pending_joiner_fd == joiner_fd) {
                LOG("Annullo richiesta precedente di %s per partita %d\n", 
                    joiner_name, clients[client_idx].game_id);
                old_game->pending_joiner_fd = -1;
                old_game->pending_joiner_name[0] = '\0';
                
                if (old_game->player1_fd >= 0) {
                    char cancel_msg[BUFFER_SIZE];
                    snprintf(cancel_msg, sizeof(cancel_msg), 
                             NOTIFY_REQUEST_CANCELLED_FMT, joiner_name);
                    send_to_client(old_game->player1_fd, cancel_msg);
                }
            }
        }
    }
    
    game_idx = find_game_index_unsafe(game_id_to_join);
    if (game_idx == -1) {
        snprintf(response_joiner, sizeof(response_joiner), "%s %d\n", ERR_GAME_NOT_FOUND, game_id_to_join);
        goto join_cleanup;
    }
    
    Game *game = &games[game_idx];
    
    // Verifica che la partita sia disponibile
    if (game->state == GAME_STATE_FINISHED) {
        snprintf(response_joiner, sizeof(response_joiner), "ERROR:Partita terminata\n");
        goto join_cleanup;
    }
    
    if (game->player1_fd == joiner_fd || game->player1_fd == -2) {
        // Caso 1: Il proprietario rientra normalmente
        if (game->player1_fd == joiner_fd) {
            LOG("Proprietario %s rientra nella partita %d\n", joiner_name, game_id_to_join);
            
            clients[client_idx].state = CLIENT_STATE_PLAYING;
            clients[client_idx].game_id = game_id_to_join;
            
            char symbol = 'X';
            char opponent_name[MAX_NAME_LEN] = "Nessuno";
            if (game->player2_name[0]) {
                strncpy(opponent_name, game->player2_name, MAX_NAME_LEN - 1);
            }
            
            snprintf(response_joiner, sizeof(response_joiner), RESP_JOIN_ACCEPTED_FMT, 
                     game_id_to_join, symbol, opponent_name);
            
            pthread_mutex_unlock(&game_list_mutex);
            pthread_mutex_unlock(&client_list_mutex);
            
            send_to_client(joiner_fd, response_joiner);
            
            pthread_mutex_lock(&game_list_mutex);
            broadcast_game_state(game_idx);
            pthread_mutex_unlock(&game_list_mutex);
            
            return;
        }
        // Caso 2: Il proprietario era disconnesso (player1_fd == -2) e ora rientra
        else if (game->player1_fd == -2 && strcmp(game->player1_name, joiner_name) == 0) {
            LOG("🔄 Proprietario %s RIENTRA dopo disconnessione nella partita %d\n", joiner_name, game_id_to_join);
            
            // Ripristina player1
            game->player1_fd = joiner_fd;
            
            // SCONGELA il turno se era congelato in attesa del proprietario
            if (game->current_turn_fd == -3) {
                game->current_turn_fd = joiner_fd;
                LOG("❄️ SCONGELATO turno del proprietario (fd %d)\n", joiner_fd);
            }
            
            clients[client_idx].state = CLIENT_STATE_PLAYING;
            clients[client_idx].game_id = game_id_to_join;
            
            char symbol = 'X';
            char opponent_name[MAX_NAME_LEN] = "Nessuno";
            if (game->player2_name[0]) {
                strncpy(opponent_name, game->player2_name, MAX_NAME_LEN - 1);
            }
            
            snprintf(response_joiner, sizeof(response_joiner), RESP_JOIN_ACCEPTED_FMT, 
                     game_id_to_join, symbol, opponent_name);
            
            pthread_mutex_unlock(&game_list_mutex);
            pthread_mutex_unlock(&client_list_mutex);
            
            send_to_client(joiner_fd, response_joiner);
            
            // Notifica all'avversario che il proprietario è rientrato
            if (game->player2_fd >= 0) {
                char notify_msg[BUFFER_SIZE];
                snprintf(notify_msg, sizeof(notify_msg), 
                         "NOTIFY:OWNER_RECONNECTED Il proprietario è rientrato. Partita ripresa.\n");
                send_to_client(game->player2_fd, notify_msg);
            }
            
            pthread_mutex_lock(&game_list_mutex);
            broadcast_game_state(game_idx);
            pthread_mutex_unlock(&game_list_mutex);
            
            return;
        }
    }
    
    // Controlla se il richiedente è player2 che rientra dopo disconnessione
    if (game->player2_fd == -2 && strcmp(game->player2_name, joiner_name) == 0) {
        LOG("🔄 Player2 %s RIENTRA dopo disconnessione nella partita %d\n", joiner_name, game_id_to_join);
        
        // Ripristina player2
        game->player2_fd = joiner_fd;
        
        // SCONGELA il turno se era congelato in attesa di player2
        if (game->current_turn_fd == -2) {
            game->current_turn_fd = joiner_fd;
            LOG("❄️ SCONGELATO turno di player2 (fd %d)\n", joiner_fd);
        }
        
        clients[client_idx].state = CLIENT_STATE_PLAYING;
        clients[client_idx].game_id = game_id_to_join;
        
        char symbol = 'O';
        char opponent_name[MAX_NAME_LEN] = "Proprietario";
        if (game->player1_name[0]) {
            strncpy(opponent_name, game->player1_name, MAX_NAME_LEN - 1);
        }
        
        snprintf(response_joiner, sizeof(response_joiner), RESP_JOIN_ACCEPTED_FMT, 
                 game_id_to_join, symbol, opponent_name);
        
        pthread_mutex_unlock(&game_list_mutex);
        pthread_mutex_unlock(&client_list_mutex);
        
        send_to_client(joiner_fd, response_joiner);
        
        // Notifica al proprietario che l'avversario è rientrato
        if (game->player1_fd >= 0) {
            char notify_msg[BUFFER_SIZE];
            snprintf(notify_msg, sizeof(notify_msg), 
                     "NOTIFY:OPPONENT_RECONNECTED L'avversario è rientrato. Partita ripresa.\n");
            send_to_client(game->player1_fd, notify_msg);
        }
        
        pthread_mutex_lock(&game_list_mutex);
        broadcast_game_state(game_idx);
        pthread_mutex_unlock(&game_list_mutex);
        
        return;
    }
    
    if (game->state == GAME_STATE_IN_PROGRESS && game->player2_fd != -1 && game->player2_fd != -2) {
        snprintf(response_joiner, sizeof(response_joiner), "ERROR:Partita piena\n");
        goto join_cleanup;
    }
    
    // Controlla se c'è già una richiesta pendente
    if (game->pending_joiner_fd >= 0) {
        snprintf(response_joiner, sizeof(response_joiner), "%s\n", ERR_ALREADY_PENDING);
        goto join_cleanup;
    }
    
    // Controlla se il creatore è già impegnato in un'altra partita
    int creator_idx = find_client_index_unsafe(game->player1_fd);
    if (creator_idx != -1 && clients[creator_idx].active) {
        if (clients[creator_idx].state == CLIENT_STATE_PLAYING) {
            // Controlla se è nella STESSA partita o in un'altra
            if (clients[creator_idx].game_id != game_id_to_join) {
                // Il creatore è già in un'ALTRA partita
                snprintf(response_joiner, sizeof(response_joiner), 
                         "ERROR:Il proprietario è già impegnato in un'altra partita\n");
                LOG("Richiesta di %s per partita %d rifiutata: creatore già impegnato in partita %d\n", 
                    joiner_name, game_id_to_join, clients[creator_idx].game_id);
                goto join_cleanup;
            }
            // Altrimenti è nella STESSA partita, può accettare la richiesta
        }
    }
    
    // Invia richiesta al creatore
    game->pending_joiner_fd = joiner_fd;
    strncpy(game->pending_joiner_name, joiner_name, MAX_NAME_LEN - 1);
    game->pending_joiner_name[MAX_NAME_LEN - 1] = '\0';
    
    // Cambia stato del richiedente
    clients[client_idx].state = CLIENT_STATE_WAITING;
    clients[client_idx].game_id = game_id_to_join;
    
    snprintf(response_joiner, sizeof(response_joiner), RESP_REQUEST_SENT_FMT, game_id_to_join);
    snprintf(notify_creator, sizeof(notify_creator), NOTIFY_JOIN_REQUEST_FMT, joiner_name);
    
    request_sent = true;
    
    //Usa owner_fd che mantiene sempre il riferimento al proprietario
    int actual_creator_fd = game->owner_fd;
    
    LOG("Richiesta join inviata da %s (fd %d) per partita %d al proprietario (owner_fd %d)\n", 
        joiner_name, joiner_fd, game_id_to_join, actual_creator_fd);
    
join_cleanup:
    pthread_mutex_unlock(&game_list_mutex);
    pthread_mutex_unlock(&client_list_mutex);
    
    if (request_sent) {
        send_to_client(joiner_fd, response_joiner);
        // MODIFICATO: Usa owner_fd direttamente
        if (actual_creator_fd >= 0) {
            send_to_client(actual_creator_fd, notify_creator);
        } else {
            LOG("⚠️ Impossibile notificare il proprietario: owner_fd non valido\n");
        }
    } else if (joiner_fd >= 0) {
        send_to_client(joiner_fd, response_joiner);
    }
}

void process_reject_command(int client_idx, const char *rejected_player_name) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS || !rejected_player_name)
        return;
    
    int creator_idx = client_idx;
    int creator_fd = -1;
    int joiner_fd = -1;
    int game_idx = -1;
    int current_game_id = -1;
    char response_creator[BUFFER_SIZE] = {0};
    char response_joiner[BUFFER_SIZE] = {0};
    bool rejected = false;
    
    pthread_mutex_lock(&client_list_mutex);
    pthread_mutex_lock(&game_list_mutex);
    
    if (!clients[creator_idx].active) {
        snprintf(response_creator, sizeof(response_creator), "ERROR:Client non attivo\n");
        goto reject_cleanup;
    }
    
    if (clients[creator_idx].state != CLIENT_STATE_LOBBY && 
        clients[creator_idx].state != CLIENT_STATE_WAITING &&
        clients[creator_idx].state != CLIENT_STATE_PLAYING) {
        snprintf(response_creator, sizeof(response_creator), 
                 "ERROR:Devi essere in LOBBY, WAITING o PLAYING per rifiutare richieste\n");
        goto reject_cleanup;
    }
    
    creator_fd = clients[creator_idx].fd;
    current_game_id = clients[creator_idx].game_id;
    game_idx = find_game_index_unsafe(current_game_id);
    
    if (game_idx == -1) {
        snprintf(response_creator, sizeof(response_creator), "%s %d\n", ERR_GAME_NOT_FOUND, current_game_id);
        goto reject_cleanup;
    }
    
    Game *game = &games[game_idx];
    
    if (game->state != GAME_STATE_WAITING) {
        snprintf(response_creator, sizeof(response_creator), "%s\n", ERR_GAME_NOT_WAITING);
        goto reject_cleanup;
    }
    
    if (game->player1_fd != creator_fd) {
        snprintf(response_creator, sizeof(response_creator), "ERROR:Not creator\n");
        goto reject_cleanup;
    }
    
    if (game->pending_joiner_fd < 0 || strcmp(game->pending_joiner_name, rejected_player_name) != 0) {
        snprintf(response_creator, sizeof(response_creator), "%s '%s'\n", ERR_NO_PENDING_REQUEST, rejected_player_name);
        goto reject_cleanup;
    }
    
    joiner_fd = game->pending_joiner_fd;
    LOG("Creatore %s HA RIFIUTATO richiesta join da %s per partita %d\n", clients[creator_idx].name, rejected_player_name, current_game_id);
    
    game->pending_joiner_fd = -1;
    game->pending_joiner_name[0] = '\0';
    
    int joiner_idx = find_client_index_unsafe(joiner_fd);
    if (joiner_idx != -1 && clients[joiner_idx].active) {
        clients[joiner_idx].state = CLIENT_STATE_LOBBY;
        clients[joiner_idx].game_id = 0;
        LOG("Joiner '%s' (fd %d) tornato a LOBBY dopo rifiuto.\n", clients[joiner_idx].name, joiner_fd);
    }
    
    snprintf(response_joiner, sizeof(response_joiner), RESP_JOIN_REJECTED_FMT, current_game_id, clients[creator_idx].name);
    snprintf(response_creator, sizeof(response_creator), RESP_REJECT_OK_FMT, rejected_player_name);
    rejected = true;
    
reject_cleanup:
    pthread_mutex_unlock(&game_list_mutex);
    pthread_mutex_unlock(&client_list_mutex);
    
    if (rejected) {
        send_to_client(joiner_fd, response_joiner);
        send_to_client(creator_fd, response_creator);
    } else if (creator_fd >= 0) {
        send_to_client(creator_fd, response_creator);
    }
}

void process_move_command(int client_idx, const char *move_args) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS || !move_args)
        return;
    
    int r, c;
    int player_fd = -1;
    int game_idx = -1;
    int current_game_id = -1;
    char response[BUFFER_SIZE] = {0};
    bool move_made = false;
    bool game_over = false;
    char game_over_status_self[20] = {0};
    char game_over_status_opponent[20] = {0};
    int opponent_fd_if_game_over = -1;
    int opponent_idx_if_game_over = -1;
    char client_name[MAX_NAME_LEN] = {0};

    if (sscanf(move_args, "%d %d", &r, &c) != 2) {
        pthread_mutex_lock(&client_list_mutex);
        if (clients[client_idx].active)
            player_fd = clients[client_idx].fd;
        pthread_mutex_unlock(&client_list_mutex);
        if (player_fd >= 0)
            snprintf(response, sizeof(response), "%s\n", ERR_INVALID_MOVE_FORMAT);
        goto move_error_exit;
    }

    pthread_mutex_lock(&client_list_mutex);
    pthread_mutex_lock(&game_list_mutex);

    if (!clients[client_idx].active) {
        goto move_cleanup;
    }
    player_fd = clients[client_idx].fd;
    strncpy(client_name, clients[client_idx].name, MAX_NAME_LEN - 1);
    client_name[MAX_NAME_LEN - 1] = '\0';

    if (clients[client_idx].state != CLIENT_STATE_PLAYING) {
        snprintf(response, sizeof(response), "%s\n", ERR_NOT_PLAYING);
        goto move_cleanup;
    }
    
    current_game_id = clients[client_idx].game_id;
    if (current_game_id <= 0) {
        snprintf(response, sizeof(response), "%s\n", ERR_NOT_PLAYING);
        goto move_cleanup;
    }
    
    game_idx = find_game_index_unsafe(current_game_id);
    if (game_idx == -1) {
        snprintf(response, sizeof(response), "%s %d\n", ERR_GAME_NOT_FOUND, current_game_id);
        clients[client_idx].state = CLIENT_STATE_LOBBY;
        clients[client_idx].game_id = 0;
        goto move_cleanup;
    }

    Game *game = &games[game_idx];
    
    if (game->state != GAME_STATE_IN_PROGRESS && game->state != GAME_STATE_WAITING) {
        snprintf(response, sizeof(response), "%s\n", ERR_GAME_NOT_IN_PROGRESS);
        goto move_cleanup;
    }
    
    if (game->player2_fd == -1) {
        if (player_fd != game->player1_fd) {
            snprintf(response, sizeof(response), "%s\n", ERR_NOT_YOUR_TURN);
            goto move_cleanup;
        }
    } else {
        if (game->current_turn_fd != player_fd) {
            snprintf(response, sizeof(response), "%s\n", ERR_NOT_YOUR_TURN);
            goto move_cleanup;
        }
    }
    
    if (r < 0 || r > 2 || c < 0 || c > 2) {
        snprintf(response, sizeof(response), "%s\n", ERR_INVALID_MOVE_BOUNDS);
        goto move_cleanup;
    }
    
    if (game->board[r][c] != CELL_EMPTY) {
        snprintf(response, sizeof(response), "%s\n", ERR_INVALID_MOVE_OCCUPIED);
        goto move_cleanup;
    }

    Cell player_symbol = (player_fd == game->player1_fd) ? CELL_X : CELL_O;
    game->board[r][c] = player_symbol;
    LOG("Giocatore '%s' (fd %d, %c) ha mosso in %d,%d nella partita %d.\n", 
        client_name, player_fd, (player_symbol == CELL_X ? 'X' : 'O'), r, c, current_game_id);
    move_made = true;
    
    opponent_fd_if_game_over = find_opponent_fd(game, player_fd);
    opponent_idx_if_game_over = find_client_index_unsafe(opponent_fd_if_game_over);

    if (check_winner(game->board, player_symbol)) {
        game_over = true;
        LOG("🏆 Partita %d: '%s' (fd %d) HA VINTO!\n", current_game_id, client_name, player_fd);

        char winner_name[MAX_NAME_LEN];
        char loser_name[MAX_NAME_LEN];
        int loser_fd = opponent_fd_if_game_over;
        int winner_fd = player_fd;
        
        strncpy(winner_name, client_name, MAX_NAME_LEN - 1);
        winner_name[MAX_NAME_LEN - 1] = '\0';
        
        if (opponent_idx_if_game_over != -1 && clients[opponent_idx_if_game_over].active) {
            strncpy(loser_name, clients[opponent_idx_if_game_over].name, MAX_NAME_LEN - 1);
            loser_name[MAX_NAME_LEN - 1] = '\0';
        } else {
            strcpy(loser_name, "Unknown");
        }
        
        // il gioco è terminato, il vincitore deciderà sul rematch
        game->state = GAME_STATE_FINISHED;
        game->player1_fd = winner_fd;
        strncpy(game->player1_name, winner_name, MAX_NAME_LEN - 1);
        game->player1_name[MAX_NAME_LEN - 1] = '\0';
        
        game->owner_fd = winner_fd;
        LOG("✅ Owner_fd aggiornato al vincitore (fd %d)\n", winner_fd);
        
        game->player2_fd = loser_fd;
        strncpy(game->player2_name, loser_name, MAX_NAME_LEN - 1);
        game->player2_name[MAX_NAME_LEN - 1] = '\0';
        
        game->current_turn_fd = -1;
        game->winner_fd = winner_fd;
        game->player1_accepted_rematch = REMATCH_CHOICE_PENDING;
        game->player2_accepted_rematch = REMATCH_CHOICE_PENDING;
        game->moves_count = 0;
        game->pending_joiner_fd = -1;
        game->pending_joiner_name[0] = '\0';
        game->loser_ready_for_rematch = false; 
        strcpy(game_over_status_self, "END_WIN");
        strcpy(game_over_status_opponent, "END_LOSE");
        
        clients[client_idx].state = CLIENT_STATE_PLAYING;
        clients[client_idx].game_id = current_game_id;
        
        if (opponent_idx_if_game_over != -1 && clients[opponent_idx_if_game_over].active) {
            clients[opponent_idx_if_game_over].state = CLIENT_STATE_PLAYING;
            clients[opponent_idx_if_game_over].game_id = current_game_id;
        }
        
        LOG("✅ Partita %d TERMINATA: Vincitore '%s' (fd %d), in attesa scelta rematch\n", 
            current_game_id, winner_name, winner_fd);
    }
    else if (board_full(game->board)) {
        game_over = true;
        LOG("🤝 Partita %d terminata con PAREGGIO tra '%s' e avversario\n", current_game_id, client_name);
        
        // Pareggio → torna in WAITING col proprietario originale
        game->state = GAME_STATE_WAITING;
        init_board(game->board);
        
        int original_owner_fd = game->owner_fd;
        char original_owner_name[MAX_NAME_LEN];
        int owner_client_idx = find_client_index_unsafe(original_owner_fd);
        
        if (owner_client_idx != -1 && clients[owner_client_idx].active) {
            strncpy(original_owner_name, clients[owner_client_idx].name, MAX_NAME_LEN - 1);
            original_owner_name[MAX_NAME_LEN - 1] = '\0';
        } else {
            strncpy(original_owner_name, game->player1_name, MAX_NAME_LEN - 1);
            original_owner_name[MAX_NAME_LEN - 1] = '\0';
        }
        
        game->player1_fd = original_owner_fd;
        strncpy(game->player1_name, original_owner_name, MAX_NAME_LEN - 1);
        game->player1_name[MAX_NAME_LEN - 1] = '\0';
        
        game->player2_fd = -1;
        game->player2_name[0] = '\0';
        
        game->current_turn_fd = original_owner_fd;
        game->winner_fd = -1;
        game->player1_accepted_rematch = REMATCH_CHOICE_PENDING;
        game->player2_accepted_rematch = REMATCH_CHOICE_PENDING;
        game->moves_count = 0;
        game->pending_joiner_fd = -1;
        game->pending_joiner_name[0] = '\0';
        
        strcpy(game_over_status_self, "DRAW");
        strcpy(game_over_status_opponent, "DRAW");
        
        // Aggiorna stati client
        clients[client_idx].state = CLIENT_STATE_LOBBY;
        if (game->owner_fd == player_fd) {
            clients[client_idx].game_id = current_game_id;
        } else {
            clients[client_idx].game_id = 0;
        }
        if (opponent_idx_if_game_over != -1 && clients[opponent_idx_if_game_over].active) {
            clients[opponent_idx_if_game_over].state = CLIENT_STATE_LOBBY;
            clients[opponent_idx_if_game_over].game_id = 0;
        }
        
        LOG("✅ Partita %d RESETTATA dopo pareggio: proprietario '%s' (fd %d), stato WAITING\n", 
            current_game_id, original_owner_name, original_owner_fd);
    }
    else {
        if (game->player2_fd == -1) {
            game->current_turn_fd = (game->current_turn_fd == game->player1_fd) ? -2 : game->player1_fd;
            if (game->current_turn_fd == -2) {
                LOG("🧊 TURNO passa a player2 (non presente) - in attesa che entri\n");
            }
        } else if (game->player2_fd == -2) {
            if (game->current_turn_fd == game->player1_fd) {
                game->current_turn_fd = -2;
                LOG("🧊 TURNO CONGELATO dopo mossa di player1: in attesa rientro player2\n");
            } else {
                game->current_turn_fd = game->player1_fd;
            }
        } else if (game->player1_fd == -2) {
            if (game->current_turn_fd == game->player2_fd) {
                game->current_turn_fd = -3;
                LOG("🧊 TURNO CONGELATO dopo mossa di player2: in attesa rientro proprietario\n");
            } else {
                game->current_turn_fd = game->player2_fd;
            }
        } else {
            game->current_turn_fd = (game->current_turn_fd == game->player1_fd) ? 
                                    game->player2_fd : game->player1_fd;
        }
    }

move_cleanup:
    pthread_mutex_unlock(&game_list_mutex);
    pthread_mutex_unlock(&client_list_mutex);

    if (move_made) {
        pthread_mutex_lock(&game_list_mutex);
        if (find_game_index_unsafe(current_game_id) == game_idx) {
            broadcast_game_state(game_idx);
        }
        pthread_mutex_unlock(&game_list_mutex);

        if (game_over) {
            char notify_self[BUFFER_SIZE], notify_opponent[BUFFER_SIZE];
            snprintf(notify_self, sizeof(notify_self), "NOTIFY:GAMEOVER %s\n", game_over_status_self);
            snprintf(notify_opponent, sizeof(notify_opponent), "NOTIFY:GAMEOVER %s\n", game_over_status_opponent);

            send_to_client(player_fd, notify_self);
            if (opponent_fd_if_game_over >= 0)
                send_to_client(opponent_fd_if_game_over, notify_opponent);

            LOG("Partita %d terminata. Vincitore: %d.\n", current_game_id, player_fd);
            
            broadcast_games_list();
        }
    }
    else {
    move_error_exit:
        if (player_fd >= 0 && response[0] != '\0') {
            send_to_client(player_fd, response);
        }
    }
}

bool process_quit_command(int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS)
        return false;
    
    int client_fd = -1;
    char client_name[MAX_NAME_LEN];
    ClientState current_state;
    int current_game_id;
    
    pthread_mutex_lock(&client_list_mutex);
    if (!clients[client_idx].active) {
        pthread_mutex_unlock(&client_list_mutex);
        return true;
    }
    client_fd = clients[client_idx].fd;
    strncpy(client_name, clients[client_idx].name, MAX_NAME_LEN - 1);
    client_name[MAX_NAME_LEN - 1] = '\0';
    current_state = clients[client_idx].state;
    current_game_id = clients[client_idx].game_id;
    pthread_mutex_unlock(&client_list_mutex);

    LOG("Client %s ha inviato QUIT. Stato: %d, ID Partita: %d\n", client_name, current_state, current_game_id);

    if (current_state == CLIENT_STATE_LOBBY) {
        LOG("Client %s è già in LOBBY, QUIT ignorato (rimane connesso)\n", client_name);
        send_to_client(client_fd, RESP_QUIT_OK);
        return false;
    }

    if (current_state == CLIENT_STATE_PLAYING || current_state == CLIENT_STATE_WAITING) {
        LOG("Client %s sta lasciando il contesto della partita %d tramite QUIT.\n", client_name, current_game_id);
        
        pthread_mutex_lock(&client_list_mutex);
        pthread_mutex_lock(&game_list_mutex);
        
        int game_idx = find_game_index_unsafe(current_game_id);
        bool is_owner = false;
        
        if (game_idx != -1 && games[game_idx].owner_fd == client_fd) {
            is_owner = true;
            LOG("🔑 Client %s è il proprietario della partita %d\n", client_name, current_game_id);
        }
        
        if (game_idx != -1) {
            handle_player_leaving_game(game_idx, client_fd, client_name);
        }
        
        if (clients[client_idx].active) {
            clients[client_idx].state = CLIENT_STATE_LOBBY;
            
            if (!is_owner) {
                clients[client_idx].game_id = 0;
                LOG("Client %s (NON proprietario) tornato in LOBBY, game_id azzerato.\n", client_name);
            } else {
                LOG("Client %s (proprietario) tornato in LOBBY, game_id=%d mantenuto.\n", client_name, current_game_id);
            }
        }
        
        pthread_mutex_unlock(&game_list_mutex);
        pthread_mutex_unlock(&client_list_mutex);
        
        send_to_client(client_fd, RESP_QUIT_OK);
        
        broadcast_games_list();
        
        return false;  
    }
    else {
        LOG("Client %s ha inviato QUIT dallo stato %d. Interpreto come disconnessione.\n", client_name, current_state);
        return true;  
    }
}

void process_rematch_command(int client_idx, const char *choice) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS || !choice)
        return;
    
    int caller_fd = -1;
    int game_idx = -1;
    int current_game_id = -1;
    char response[BUFFER_SIZE] = {0};
    int opponent_fd = -1;
    int opponent_idx = -1;
    
    pthread_mutex_lock(&client_list_mutex);
    if (!clients[client_idx].active) {
        pthread_mutex_unlock(&client_list_mutex);
        return;
    }
    caller_fd = clients[client_idx].fd;
    current_game_id = clients[client_idx].game_id;
    pthread_mutex_unlock(&client_list_mutex);
    
    bool is_yes = (strstr(choice, "YES") != NULL) || (strstr(choice, "Yes") != NULL) || (strstr(choice, "yes") != NULL);
    bool is_no  = (strstr(choice, "NO")  != NULL) || (strstr(choice, "No")  != NULL) || (strstr(choice, "no")  != NULL);

    pthread_mutex_lock(&game_list_mutex);
    game_idx = find_game_index_unsafe(current_game_id);
    
    if (game_idx == -1) {
        pthread_mutex_unlock(&game_list_mutex);
        send_to_client(caller_fd, "ERROR:Partita non trovata\n");
        return;
    }
    
    Game *game = &games[game_idx];
    
    if (game->state != GAME_STATE_FINISHED || game->winner_fd != caller_fd) {
        pthread_mutex_unlock(&game_list_mutex);
        send_to_client(caller_fd, "ERROR:Comando rematch non valido nello stato attuale della partita\n");
        return;
    }
    
    opponent_fd = find_opponent_fd(game, caller_fd);
    opponent_idx = find_client_index_unsafe(opponent_fd);
    
    if (is_no) {
        LOG("🗑️ VINCITORE RIFIUTA REMATCH: Elimino partita %d\n", current_game_id);
        
        // Notifica l'avversario (perdente) e riporta in lobby se possibile
        if (opponent_fd >= 0 && opponent_idx != -1 && clients[opponent_idx].active) {
            send_to_client(opponent_fd, "NOTIFY:REMATCH_DECLINED Il vincitore ha rifiutato il rematch. Tornare alla lobby.\n");
            pthread_mutex_lock(&client_list_mutex);
            clients[opponent_idx].state = CLIENT_STATE_LOBBY;
            clients[opponent_idx].game_id = 0;
            pthread_mutex_unlock(&client_list_mutex);
        }
        
        // Riporta vincitore in lobby (azzerando game_id)
        pthread_mutex_lock(&client_list_mutex);
        int winner_idx = find_client_index_unsafe(caller_fd);
        if (winner_idx != -1 && clients[winner_idx].active) {
            clients[winner_idx].state = CLIENT_STATE_LOBBY;
            clients[winner_idx].game_id = 0;
        }
        pthread_mutex_unlock(&client_list_mutex);
        
        // Elimina la partita dallo slot
        reset_game_slot_to_empty_unsafe(game_idx);
        
        pthread_mutex_unlock(&game_list_mutex);
        
        snprintf(response, sizeof(response), "%s\n", RESP_REMATCH_DECLINED);
        send_to_client(caller_fd, response);
        
        // Aggiorna lista giochi 
        broadcast_games_list();
        return;
    }
    else if (is_yes) {
        LOG("✅ VINCITORE ACCETTA REMATCH: Partita %d torna in WAITING\n", current_game_id);
        
        game->state = GAME_STATE_WAITING;
        init_board(game->board);
        
        game->player1_fd = game->owner_fd;
        int owner_client_idx = find_client_index_unsafe(game->owner_fd);
        if (owner_client_idx != -1 && clients[owner_client_idx].active) {
            strncpy(game->player1_name, clients[owner_client_idx].name, MAX_NAME_LEN - 1);
            game->player1_name[MAX_NAME_LEN - 1] = '\0';
        }
        
        game->player2_fd = -1;
        game->player2_name[0] = '\0';
        
        // reset campi rematch/turni
        game->current_turn_fd = game->owner_fd;
        game->winner_fd = -1;
        game->player1_accepted_rematch = REMATCH_CHOICE_PENDING;
        game->player2_accepted_rematch = REMATCH_CHOICE_PENDING;
        game->moves_count = 0;
        game->pending_joiner_fd = -1;
        game->pending_joiner_name[0] = '\0';
        
        // Aggiorna stati client
        pthread_mutex_lock(&client_list_mutex);
        int winner_idx = find_client_index_unsafe(caller_fd);
        if (winner_idx != -1 && clients[winner_idx].active) {
            clients[winner_idx].state = CLIENT_STATE_LOBBY;
            clients[winner_idx].game_id = current_game_id;
        }
        // il perdente torna in LOBBY senza game_id
        if (opponent_idx != -1 && clients[opponent_idx].active) {
            clients[opponent_idx].state = CLIENT_STATE_LOBBY;
            clients[opponent_idx].game_id = 0;
            
            if (game->loser_ready_for_rematch) {
                LOG("✅ Perdente pronto, invio NOTIFY:OPPONENT_ACCEPTED_REMATCH a fd %d\n", opponent_fd);
                char notify_loser[BUFFER_SIZE];
                snprintf(notify_loser, sizeof(notify_loser), NOTIFY_OPPONENT_ACCEPTED_REMATCH, current_game_id);
                send_to_client(opponent_fd, notify_loser);
            } else {
                LOG("⏸️ Perdente NON ancora pronto (non ha cliccato OK), invito rematch in coda\n");
            }
        }
        pthread_mutex_unlock(&client_list_mutex);
        
        pthread_mutex_unlock(&game_list_mutex);
        
        // Risposta al vincitore
        char resp[BUFFER_SIZE];
        snprintf(resp, sizeof(resp), RESP_REMATCH_ACCEPTED_FMT, current_game_id);
        send_to_client(caller_fd, resp);
        
        // Aggiorna lista giochi 
        broadcast_games_list();
        return;
    }
    else {
        pthread_mutex_unlock(&game_list_mutex);
        send_to_client(caller_fd, "ERROR:Scelta rematch non valida (usa YES o NO)\n");
        return;
    }
}

// Gestisce ACK_GAMEOVER dal vincitore
void process_ack_gameover(int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS)
        return;
    
    int caller_fd = -1;
    int current_game_id = -1;
    int game_idx = -1;
    
    pthread_mutex_lock(&client_list_mutex);
    if (!clients[client_idx].active) {
        pthread_mutex_unlock(&client_list_mutex);
        return;
    }
    caller_fd = clients[client_idx].fd;
    current_game_id = clients[client_idx].game_id;
    pthread_mutex_unlock(&client_list_mutex);
    
    LOG("📨 Ricevuto ACK_GAMEOVER da fd %d per partita %d\n", caller_fd, current_game_id);
    
    pthread_mutex_lock(&game_list_mutex);
    game_idx = find_game_index_unsafe(current_game_id);
    
    if (game_idx == -1) {
        pthread_mutex_unlock(&game_list_mutex);
        LOG("⚠️ ACK_GAMEOVER: Partita %d non trovata\n", current_game_id);
        return;
    }
    
    Game *game = &games[game_idx];
    
    // Verifica che sia una partita FINISHED e che il caller sia il vincitore
    if (game->state != GAME_STATE_FINISHED || game->winner_fd != caller_fd) {
        pthread_mutex_unlock(&game_list_mutex);
        LOG("⚠️ ACK_GAMEOVER: Stato partita non valido o caller non è il vincitore\n");
        return;
    }
    
    pthread_mutex_unlock(&game_list_mutex);
    
    LOG("✅ Invio CMD:REMATCH_OFFER al vincitore (fd %d) dopo ACK\n", caller_fd);
    send_to_client(caller_fd, CMD_REMATCH_OFFER);
}

void process_accept_command(int client_idx, const char *accepted_player_name) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS || !accepted_player_name)
        return;
    
    int creator_idx = client_idx;
    int creator_fd = -1;
    int joiner_fd = -1;
    int game_idx = -1;
    int current_game_id = -1;
    char response_creator[BUFFER_SIZE] = {0};
    char response_joiner[BUFFER_SIZE] = {0};
    char notify_creator_start[BUFFER_SIZE] = {0};
    char notify_joiner_start[BUFFER_SIZE] = {0};
    bool accepted = false;
    
    pthread_mutex_lock(&client_list_mutex);
    pthread_mutex_lock(&game_list_mutex);
    
    if (!clients[creator_idx].active) {
        snprintf(response_creator, sizeof(response_creator), "ERROR:Client non attivo\n");
        goto accept_cleanup;
    }
    
    LOG("🔍 DEBUG ACCEPT: client_idx=%d, fd=%d, nome='%s', stato=%d\n", 
        creator_idx, clients[creator_idx].fd, clients[creator_idx].name, clients[creator_idx].state);
    
    if (clients[creator_idx].state != CLIENT_STATE_LOBBY && 
        clients[creator_idx].state != CLIENT_STATE_PLAYING) {
        snprintf(response_creator, sizeof(response_creator), 
                 "ERROR:Devi essere in LOBBY o in partita per accettare richieste\n");
        goto accept_cleanup;
    }
    
    creator_fd = clients[creator_idx].fd;
    current_game_id = clients[creator_idx].game_id;
    
    LOG("🔍 DEBUG ACCEPT: current_game_id=%d\n", current_game_id);
    
    if (current_game_id <= 0) {
        snprintf(response_creator, sizeof(response_creator), "ERROR:Non hai partite create\n");
        goto accept_cleanup;
    }
    
    game_idx = find_game_index_unsafe(current_game_id);
    
    if (game_idx == -1) {
        LOG("🔍 DEBUG ACCEPT: Partita %d NON TROVATA!\n", current_game_id);
        snprintf(response_creator, sizeof(response_creator), "%s %d\n", ERR_GAME_NOT_FOUND, current_game_id);
        goto accept_cleanup;
    }
    
    Game *game = &games[game_idx];
    
    LOG("🔍 DEBUG ACCEPT: Partita trovata: id=%d, player1_fd=%d, owner_fd=%d, player1_name='%s'\n",
        game->id, game->player1_fd, game->owner_fd, game->player1_name);
    LOG("🔍 DEBUG ACCEPT: pending_joiner_fd=%d, pending_joiner_name='%s'\n",
        game->pending_joiner_fd, game->pending_joiner_name);
    LOG("🔍 DEBUG ACCEPT: accepted_player_name='%s'\n", accepted_player_name);
    
    if (game->owner_fd != creator_fd) {
        LOG("🔍 DEBUG ACCEPT: ERRORE - Non sei il proprietario! owner_fd=%d, creator_fd=%d\n",
            game->owner_fd, creator_fd);
        snprintf(response_creator, sizeof(response_creator), "ERROR:Non sei il proprietario di questa partita\n");
        goto accept_cleanup;
    }
    
    if (game->pending_joiner_fd < 0) {
        LOG("🔍 DEBUG ACCEPT: ERRORE - Nessuna richiesta pendente (pending_joiner_fd=%d)\n",
            game->pending_joiner_fd);
        snprintf(response_creator, sizeof(response_creator), "%s '%s'\n", ERR_NO_PENDING_REQUEST, accepted_player_name);
        goto accept_cleanup;
    }
    
    if (strcmp(game->pending_joiner_name, accepted_player_name) != 0) {
        LOG("🔍 DEBUG ACCEPT: ERRORE - Nome non corrisponde! pending='%s', accepted='%s'\n",
            game->pending_joiner_name, accepted_player_name);
        snprintf(response_creator, sizeof(response_creator), "%s '%s'\n", ERR_NO_PENDING_REQUEST, accepted_player_name);
        goto accept_cleanup;
    }
    
    joiner_fd = game->pending_joiner_fd;
    int joiner_idx = find_client_index_unsafe(joiner_fd);
    
    if (joiner_idx == -1 || !clients[joiner_idx].active) {
        snprintf(response_creator, sizeof(response_creator), "%s\n", ERR_JOINER_LEFT);
        game->pending_joiner_fd = -1;
        game->pending_joiner_name[0] = '\0';
        goto accept_cleanup;
    }
    
    LOG("Creatore %s HA ACCETTATO richiesta join da %s per partita %d\n", 
        clients[creator_idx].name, accepted_player_name, current_game_id);
    
    if (game->player1_fd == -2 || game->player1_fd != creator_fd) {
        game->player1_fd = creator_fd;
        LOG("✅ Ripristinato player1_fd del proprietario (fd %d)\n", creator_fd);
    }
    
    game->player2_fd = joiner_fd;
    strncpy(game->player2_name, accepted_player_name, MAX_NAME_LEN - 1);
    game->player2_name[MAX_NAME_LEN - 1] = '\0';
    game->state = GAME_STATE_IN_PROGRESS;
    

    if (game->current_turn_fd == -2) {
        game->current_turn_fd = game->player2_fd;
        LOG("✅ Turno SCONGELATO: era in attesa di player2, ora è il turno di player2 (fd %d)\n", game->player2_fd);
    } else if (game->current_turn_fd == game->player1_fd) {
        LOG("✅ Partita iniziata - Primo turno del proprietario (fd %d)\n", game->player1_fd);
    } else {
        game->current_turn_fd = game->player1_fd;
        LOG("✅ Partita iniziata - Turno impostato al proprietario (fd %d)\n", game->player1_fd);
    }
    
    game->pending_joiner_fd = -1;
    game->pending_joiner_name[0] = '\0';
    
    clients[joiner_idx].state = CLIENT_STATE_PLAYING;
    clients[joiner_idx].game_id = current_game_id;
    
    clients[creator_idx].state = CLIENT_STATE_PLAYING;
    
    snprintf(response_joiner, sizeof(response_joiner), RESP_JOIN_ACCEPTED_FMT, 
             current_game_id, 'O', game->player1_name);
    snprintf(notify_joiner_start, sizeof(notify_joiner_start), NOTIFY_GAME_START_FMT, 
             current_game_id, 'O', game->player1_name);
    snprintf(notify_creator_start, sizeof(notify_creator_start), NOTIFY_GAME_START_FMT, 
             current_game_id, 'X', game->player2_name);
    
    accepted = true;
    
accept_cleanup:
    pthread_mutex_unlock(&game_list_mutex);
    pthread_mutex_unlock(&client_list_mutex);
    
    if (accepted) {
        send_to_client(joiner_fd, response_joiner);
        send_to_client(joiner_fd, notify_joiner_start);
        send_to_client(creator_fd, notify_creator_start);
        
        pthread_mutex_lock(&game_list_mutex);
        if (game_idx != -1) {
            broadcast_game_state(game_idx);
        }
        pthread_mutex_unlock(&game_list_mutex);
        
        broadcast_games_list();
    } else if (creator_fd >= 0) {
        send_to_client(creator_fd, response_creator);
    }
}

void send_unknown_command_error(int client_idx, const char *received_command, ClientState current_state) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS)
        return;
    
    int client_fd = -1;
    pthread_mutex_lock(&client_list_mutex);
    if (clients[client_idx].active)
        client_fd = clients[client_idx].fd;
    pthread_mutex_unlock(&client_list_mutex);
    
    if (client_fd >= 0) {
        char error_resp[BUFFER_SIZE];
        snprintf(error_resp, sizeof(error_resp), ERR_UNKNOWN_COMMAND_FMT,
                 current_state, received_command ? received_command : "<empty>");
        strncat(error_resp, "\n", sizeof(error_resp) - strlen(error_resp) - 1);
        send_to_client(client_fd, error_resp);
    }
}
void process_ack_gameover_loser(int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_TOTAL_CLIENTS)
        return;
    
    int caller_fd = -1;
    int current_game_id = -1;
    int game_idx = -1;
    
    pthread_mutex_lock(&client_list_mutex);
    if (!clients[client_idx].active) {
        pthread_mutex_unlock(&client_list_mutex);
        return;
    }
    caller_fd = clients[client_idx].fd;
    current_game_id = clients[client_idx].game_id;
    pthread_mutex_unlock(&client_list_mutex);
    
    LOG("📨 Ricevuto ACK_GAMEOVER_LOSER da perdente (fd %d) per partita %d - Pronto per invito rematch\n", 
        caller_fd, current_game_id);
    
    pthread_mutex_lock(&game_list_mutex);
    game_idx = find_game_index_unsafe(current_game_id);
    
    if (game_idx != -1) {
        games[game_idx].loser_ready_for_rematch = true;
        LOG("✅ Flag loser_ready_for_rematch impostato a TRUE per partita %d\n", current_game_id);

        if (games[game_idx].state == GAME_STATE_WAITING) {
            LOG("✅ Partita %d già in WAITING, invio invito rematch al perdente (fd %d)\n", 
                current_game_id, caller_fd);
            
            char notify_loser[BUFFER_SIZE];
            snprintf(notify_loser, sizeof(notify_loser), NOTIFY_OPPONENT_ACCEPTED_REMATCH, current_game_id);
            
            pthread_mutex_unlock(&game_list_mutex);
            send_to_client(caller_fd, notify_loser);
            return;
        }
    }
    
    pthread_mutex_unlock(&game_list_mutex);
}
