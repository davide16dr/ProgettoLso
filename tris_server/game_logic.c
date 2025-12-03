#include "game_logic.h"
#include "utils.h"
#include "protocol.h"
#include <string.h>
#include <stdio.h>

const char *NOTIFY_OPPONENT_LEFT = "NOTIFY:OPPONENT_LEFT Back to lobby.\n";
const char *NOTIFY_WINNER_LEFT_AFTER_GAME = "NOTIFY:WINNER_LEFT Back to lobby.\n";
const char *NOTIFY_GAMEOVER_WIN = "NOTIFY:GAMEOVER WIN\n";
const char *NOTIFY_GAMEOVER_LOSE = "NOTIFY:GAMEOVER LOSE\n";
const char *NOTIFY_GAMEOVER_DRAW = "NOTIFY:GAMEOVER DRAW\n";
const char *NOTIFY_YOUR_TURN = "NOTIFY:YOUR_TURN\n";
const char *NOTIFY_BOARD_PREFIX = "NOTIFY:BOARD ";
const char *RESP_ERROR_PREFIX = "ERROR:";

void init_board(Cell board[3][3])
{
    memset(board, CELL_EMPTY, sizeof(Cell) * 3 * 3);
}

void board_to_string(const Cell board[3][3], char *out_str, size_t max_len)
{
    if (!out_str || max_len == 0)
        return;
    out_str[0] = '\0';
    size_t current_len = 0;
    const size_t cell_width = 2;

    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            size_t needed = (r == 2 && c == 2) ? 1 : cell_width;
            if (current_len + needed >= max_len)
            {
                out_str[max_len - 1] = '\0';
                LOG("Attenzione: buffer board_to_string troncato.\n");
                return;
            }

            char cell_char;
            switch (board[r][c])
            {
            case CELL_X:
                cell_char = 'X';
                break;
            case CELL_O:
                cell_char = 'O';
                break;
            default:
                cell_char = '-';
                break;
            }

            out_str[current_len++] = cell_char;

            if (r < 2 || c < 2)
            {
                out_str[current_len++] = ' ';
            }
        }
    }
    out_str[current_len] = '\0';
}

bool check_winner(const Cell board[3][3], Cell player)
{
    if (player == CELL_EMPTY)
        return false;

    for (int i = 0; i < 3; i++)
    {
        if (board[i][0] == player && board[i][1] == player && board[i][2] == player)
            return true;
        if (board[0][i] == player && board[1][i] == player && board[2][i] == player)
            return true;
    }

    if (board[0][0] == player && board[1][1] == player && board[2][2] == player)
        return true;
    if (board[0][2] == player && board[1][1] == player && board[2][0] == player)
        return true;
    return false;
}

bool board_full(const Cell board[3][3])
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (board[i][j] == CELL_EMPTY)
                return false;
    return true;
}

int find_game_index_unsafe(int game_id)
{
    if (game_id <= 0)
        return -1;
    for (int i = 0; i < MAX_GAMES; ++i)
    {
        if (games[i].state != GAME_STATE_EMPTY && games[i].id == game_id)
        {
            return i;
        }
    }
    return -1;
}

int find_client_index_unsafe(int fd)
{
    if (fd < 0)
        return -1;
    for (int i = 0; i < MAX_TOTAL_CLIENTS; ++i)
    {
        if (clients[i].active && clients[i].fd == fd)
        {
            return i;
        }
    }
    return -1;
}

int find_opponent_fd(const Game *game, int player_fd)
{
    if (!game || player_fd < 0)
        return -1;
    if (game->player1_fd == player_fd)
        return game->player2_fd;
    if (game->player2_fd == player_fd)
        return game->player1_fd;
    return -1;
}

void reset_game_slot_to_empty_unsafe(int game_idx)
{
    if (game_idx < 0 || game_idx >= MAX_GAMES)
        return;

    if (games[game_idx].id > 0)
    {
        LOG("Reimposto lo slot partita indice %d (ID Partita Precedente: %d) a EMPTY\n", game_idx, games[game_idx].id);
    }

    memset(&games[game_idx], 0, sizeof(Game));

    games[game_idx].id = 0;
    games[game_idx].state = GAME_STATE_EMPTY;
    games[game_idx].player1_fd = -1;
    games[game_idx].player2_fd = -1;
    games[game_idx].current_turn_fd = -1;
    games[game_idx].pending_joiner_fd = -1;
    games[game_idx].winner_fd = -1;
    games[game_idx].owner_fd = -1;
}

bool handle_player_leaving_game(int game_idx, int leaving_client_fd, const char *leaving_client_name)
{
    if (game_idx < 0 || game_idx >= MAX_GAMES || leaving_client_fd < 0)
    {
        LOG("Errore: Argomenti non validi per handle_player_leaving_game (game_idx=%d, fd=%d)\n", game_idx, leaving_client_fd);
        return false;
    }

    Game *game = &games[game_idx];
    GameState state_at_entry = game->state;

    if (state_at_entry == GAME_STATE_EMPTY)
    {
        LOG("handle_player_leaving_game chiamato per uno slot già EMPTY (idx %d). Ignoro.\n", game_idx);
        return false;
    }

    char response[BUFFER_SIZE];
    bool game_reset_to_empty = false;

    int opponent_fd = -1;
    int opponent_idx = -1;
    bool was_player1 = (game->player1_fd == leaving_client_fd);
    bool was_player2 = (game->player2_fd == leaving_client_fd);
    bool was_pending = (game->pending_joiner_fd == leaving_client_fd);

    LOG("🔄 Gestione uscita: Giocatore '%s' (fd %d) dalla partita %d (stato %d)\n",
        leaving_client_name ? leaving_client_name : "N/A",
        leaving_client_fd, game->id, state_at_entry);

    // CASO 1: Richiedente in attesa esce
    if (was_pending && state_at_entry == GAME_STATE_WAITING)
    {
        LOG("Richiedente in attesa '%s' ha lasciato - pulisco richiesta.\n", game->pending_joiner_name);
        game->pending_joiner_fd = -1;
        game->pending_joiner_name[0] = '\0';
        
        if (game->player1_fd >= 0)
        {
            snprintf(response, sizeof(response), NOTIFY_REQUEST_CANCELLED_FMT,
                     leaving_client_name ? leaving_client_name : "Player");
            send_to_client(game->player1_fd, response);
        }
        return false;
    }
    
    // CASO 2: Proprietario esce da partita WAITING (senza player2 attivo)
    if (was_player1 && state_at_entry == GAME_STATE_WAITING && game->player2_fd == -1)
    {
        LOG("⏸️ Proprietario '%s' si disconnette da partita WAITING - MANTIENI partita %d aperta\n", 
            leaving_client_name, game->id);
        
        // Proprietario disconnesso
        game->player1_fd = -2;
        
        int pending_joiner_notify_fd = game->pending_joiner_fd;
        
        if (pending_joiner_notify_fd >= 0)
        {
            snprintf(response, sizeof(response), 
                     "NOTIFY:OWNER_DISCONNECTED Il proprietario si è disconnesso temporaneamente. Puoi aspettare o annullare.\n");
            send_to_client(pending_joiner_notify_fd, response);
        }
        
        LOG("✅ Partita %d MANTENUTA: Proprietario disconnesso, griglia salvata, stato WAITING\n", game->id);
        return false;
    }
    
    // CASO 3: Player2 esce da partita IN_PROGRESS
    if (was_player2 && state_at_entry == GAME_STATE_IN_PROGRESS)
    {
        LOG("⏸️ Player2 '%s' esce dalla partita %d - CONGELO turno e aspetto rientro\n", 
            leaving_client_name, game->id);
        
        opponent_fd = game->player1_fd;
        opponent_idx = find_client_index_unsafe(opponent_fd);
        
        int saved_turn_fd = game->current_turn_fd;
        bool was_player2_turn = (saved_turn_fd == game->player2_fd);
        
        // Player2 disconnesso
        game->player2_fd = -2;
        
        if (was_player2_turn)
        {
            // Era il turno di player2, CONGELA il turno
            game->current_turn_fd = -2; 
            LOG("🧊 TURNO CONGELATO: Era turno di player2 - in attesa del suo rientro\n");
            
            if (opponent_fd >= 0)
            {
                snprintf(response, sizeof(response), 
                         "NOTIFY:OPPONENT_DISCONNECTED L'avversario si è disconnesso durante il suo turno. Partita in pausa.\n");
                send_to_client(opponent_fd, response);
            }
        }
        else
        {
            LOG("✅ Proprietario può ancora giocare il suo turno\n");
            
            if (opponent_fd >= 0)
            {
                snprintf(response, sizeof(response), 
                         "NOTIFY:OPPONENT_DISCONNECTED L'avversario si è disconnesso. Puoi completare il turno, poi partita in pausa.\n");
                send_to_client(opponent_fd, response);
            }
        }
        
        if (opponent_fd >= 0)
        {
            broadcast_game_state(game_idx);
        }
        
        return false;
    }
    
    // CASO 4: Proprietario esce da partita IN_PROGRESS (con player2 attivo)
    if (was_player1 && state_at_entry == GAME_STATE_IN_PROGRESS && game->player2_fd >= 0)
    {
        LOG("⏸️ Proprietario '%s' esce dalla partita %d - CONGELO turno e aspetto rientro\n", 
            leaving_client_name, game->id);
        
        opponent_fd = game->player2_fd;
        opponent_idx = find_client_index_unsafe(opponent_fd);
        
        // Salva il turno corrente prima di rimuovere player1
        int saved_turn_fd = game->current_turn_fd;
        bool was_player1_turn = (saved_turn_fd == game->player1_fd);
        
        // Proprietario disconnesso
        game->player1_fd = -2; 
        
        // GESTIONE INTELLIGENTE DEL TURNO:
        if (was_player1_turn)
        {
            // Era il turno del proprietario, CONGELA il turno
            game->current_turn_fd = -3;  // -3 in attesa del proprietario
            LOG("🧊 TURNO CONGELATO: Era turno del proprietario - in attesa del suo rientro\n");
            
            if (opponent_fd >= 0)
            {
                snprintf(response, sizeof(response), 
                         "NOTIFY:OWNER_DISCONNECTED Il proprietario si è disconnesso durante il suo turno. Partita in pausa.\n");
                send_to_client(opponent_fd, response);
            }
        }
        else
        {
            LOG("✅ Avversario può ancora giocare il suo turno\n");
            
            if (opponent_fd >= 0)
            {
                snprintf(response, sizeof(response), 
                         "NOTIFY:OWNER_DISCONNECTED Il proprietario si è disconnesso. Puoi completare il turno, poi partita in pausa.\n");
                send_to_client(opponent_fd, response);
            }
        }
        
        if (opponent_fd >= 0)
        {
            broadcast_game_state(game_idx);
        }
        
        return false;
    }
    
    // CASO 5: Player2 esce da partita WAITING (era entrato ma non ha ancora giocato)
    if (was_player2 && state_at_entry == GAME_STATE_WAITING)
    {
        LOG("⏸️ Player2 '%s' esce dalla partita %d WAITING - rimuovo player2\n", 
            leaving_client_name, game->id);
        
        game->player2_fd = -1;
        game->player2_name[0] = '\0';
        
        if (game->player1_fd >= 0)
        {
            snprintf(response, sizeof(response), 
                     "NOTIFY:OPPONENT_DISCONNECTED L'avversario è uscito prima di iniziare.\n");
            send_to_client(game->player1_fd, response);
        }
        
        return false;
    }
    
    // CASO 6: Giocatori escono da partita FINISHED
    if ((was_player1 || was_player2) && state_at_entry == GAME_STATE_FINISHED)
    {
        LOG("🏁 Giocatore '%s' esce da partita FINISHED %d\n", leaving_client_name, game->id);
        
        opponent_fd = find_opponent_fd(game, leaving_client_fd);
        opponent_idx = find_client_index_unsafe(opponent_fd);

        bool is_winner = (game->winner_fd == leaving_client_fd);
        bool is_draw = (game->winner_fd == -1);

        if (was_player1)
            game->player1_fd = -2;
        else
            game->player2_fd = -2;

        if (is_winner)
        {
            LOG("🗑️ VINCITORE esce - ELIMINO partita %d\n", game->id);
            
            if (opponent_fd >= 0 && opponent_idx != -1 && clients[opponent_idx].active)
            {
                send_to_client(opponent_fd, NOTIFY_WINNER_LEFT_AFTER_GAME);
                clients[opponent_idx].state = CLIENT_STATE_LOBBY;
                clients[opponent_idx].game_id = 0;
            }
            
            reset_game_slot_to_empty_unsafe(game_idx);
            game_reset_to_empty = true;
        }
        else if (is_draw && opponent_fd == -2)
        {
            LOG("🗑️ Entrambi i giocatori usciti dopo pareggio - ELIMINO partita %d\n", game->id);
            reset_game_slot_to_empty_unsafe(game_idx);
            game_reset_to_empty = true;
        }
        else
        {
            LOG("⏸️ Perdente esce - partita %d rimane per il vincitore\n", game->id);
        }
        
        return game_reset_to_empty;
    }
    
    // CASO DEFAULT: Situazione inattesa
    LOG("⚠️ Situazione inattesa per giocatore '%s' (fd %d) partita %d stato %d\n",
        leaving_client_name ? leaving_client_name : "Player",
        leaving_client_fd, game->id, state_at_entry);

    return game_reset_to_empty;
}

void broadcast_game_state(int game_idx)
{
    if (game_idx < 0 || game_idx >= MAX_GAMES)
    {
        LOG("Errore: game_idx %d non valido per broadcast_game_state\n", game_idx);
        return;
    }

    Game *game = &games[game_idx];

    if (game->state == GAME_STATE_EMPTY)
    {
        LOG("Attenzione: Tentativo di broadcast_game_state per lo slot EMPTY indice %d\n", game_idx);
        return;
    }

    char board_msg[BUFFER_SIZE];
    char board_str[20];

    board_to_string(game->board, board_str, sizeof(board_str));
    snprintf(board_msg, sizeof(board_msg), "%s%s\n", NOTIFY_BOARD_PREFIX, board_str);

    if (game->player1_fd >= 0)
    {
        send_to_client(game->player1_fd, board_msg);
    }
    if (game->player2_fd >= 0)
    {
        send_to_client(game->player2_fd, board_msg);
    }

    if ((game->state == GAME_STATE_IN_PROGRESS || game->state == GAME_STATE_WAITING) && 
        game->current_turn_fd >= 0)
    {
        // Caso 1: Entrambi i giocatori presenti
        if (game->player1_fd >= 0 && game->player2_fd >= 0)
        {
            LOG("--- BROADCAST: Invio YOUR_TURN a fd %d per partita %d (Stato: %d, Entrambi presenti) ---\n",
                game->current_turn_fd, game->id, game->state);
            send_to_client(game->current_turn_fd, NOTIFY_YOUR_TURN);
        }
        // Caso 2: Solo player1 presente
        else if (game->player1_fd >= 0 && game->player2_fd == -1 && game->current_turn_fd == game->player1_fd)
        {
            LOG("--- BROADCAST: Invio YOUR_TURN a player1 (fd %d) per partita %d (Stato: %d, Gioca da solo) ---\n",
                game->current_turn_fd, game->id, game->state);
            send_to_client(game->current_turn_fd, NOTIFY_YOUR_TURN);
        }
        else
        {
            LOG("--- BROADCAST: NON invio YOUR_TURN per partita %d (Stato: %d, Situazione non valida) ---\n",
                game->id, game->state);
        }
    }
    else
    {
        char p1n[10], p2n[10];
        snprintf(p1n, sizeof(p1n), "%d", game->player1_fd);
        snprintf(p2n, sizeof(p2n), "%d", game->player2_fd);
        LOG("--- BROADCAST: NON invio YOUR_TURN per partita %d (Stato: %d, Turno FD: %d, P1_FD: %s, P2_FD: %s) ---\n",
            game->id, game->state, game->current_turn_fd,
            p1n, p2n);
    }
}