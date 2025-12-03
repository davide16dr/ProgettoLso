# 🎮 Tris Server - Documentazione Tecnica

## 📋 Indice
1. [Panoramica](#panoramica)
2. [Architettura](#architettura)
3. [Strutture Dati](#strutture-dati)
4. [Protocollo di Comunicazione](#protocollo-di-comunicazione)
5. [Logica di Gioco](#logica-di-gioco)
6. [Gestione Connessioni](#gestione-connessioni)
7. [Sistema Rematch](#sistema-rematch)
8. [Compilazione ed Esecuzione](#compilazione-ed-esecuzione)

---

## 📖 Panoramica

Il **Tris Server** è un server multi-thread TCP/IP scritto in C che gestisce partite di Tris multiplayer. Supporta:

- ✅ **Connessioni simultanee**: Fino a 20 client connessi contemporaneamente
- ✅ **Partite multiple**: Fino a 20 partite attive in parallelo
- ✅ **Riconnessione automatica**: I giocatori possono disconnettersi e riconnettersi senza perdere la partita
- ✅ **Sistema Rematch**: Possibilità di giocare di nuovo dopo una partita
- ✅ **Thread-safe**: Gestione sicura delle risorse condivise con mutex

---

## 🏗️ Architettura

### Moduli del Server

```
tris_server/
├── server.c          # Entry point, gestione connessioni TCP
├── client_h.c/h      # Handler dei client (thread per ogni client)
├── protocol.c/h      # Protocollo di comunicazione e comandi
├── game_logic.c/h    # Logica del gioco del Tris
├── types.h           # Definizioni di tipi e strutture dati
├── utils.c/h         # Funzioni di utilità (log, send)
└── globals.c         # Variabili globali condivise
```

### Thread Model

```
┌─────────────────────────────────────────┐
│         MAIN THREAD (server.c)          │
│  - accept() nuove connessioni           │
│  - crea thread per ogni client          │
└─────────────────────────────────────────┘
                    │
        ┌───────────┴───────────┐
        │                       │
┌───────▼──────────┐  ┌─────────▼────────┐
│  CLIENT THREAD 1 │  │ CLIENT THREAD 2  │
│  (client_h.c)    │  │  (client_h.c)    │
│  - read()        │  │  - read()        │
│  - handleMessage │  │  - handleMessage │
│  - send()        │  │  - send()        │
└──────────────────┘  └──────────────────┘
```

### Sincronizzazione

Il server utilizza **2 mutex globali** per proteggere le risorse condivise:

```c
pthread_mutex_t client_list_mutex;  // Protegge array clients[]
pthread_mutex_t game_list_mutex;    // Protegge array games[]
```

**Regola d'oro**: Acquisire **sempre** i mutex in quest'ordine per evitare deadlock:
1. `client_list_mutex` (PRIMA)
2. `game_list_mutex` (DOPO)

---

## 📊 Strutture Dati

### 1. Client (`types.h`)

```c
typedef enum {
    CLIENT_STATE_CONNECTED,  // Appena connesso, deve inviare nome
    CLIENT_STATE_LOBBY,      // In lobby, può creare/unirsi a partite
    CLIENT_STATE_WAITING,    // Ha inviato richiesta join, aspetta risposta
    CLIENT_STATE_PLAYING     // Sta giocando una partita
} ClientState;

typedef struct {
    int fd;                      // File descriptor della socket
    ClientState state;           // Stato corrente del client
    char name[MAX_NAME_LEN];     // Nome del giocatore (univoco)
    int game_id;                 // ID della partita corrente (0 = nessuna)
    bool active;                 // true se la connessione è attiva
    pthread_t thread_id;         // Thread che gestisce questo client
} Client;
```

**Array globale**: `Client clients[MAX_TOTAL_CLIENTS]` (MAX = 20)

### 2. Game (`types.h`)

```c
typedef enum {
    GAME_STATE_EMPTY,        // Slot vuoto (partita non creata)
    GAME_STATE_WAITING,      // In attesa del secondo giocatore
    GAME_STATE_IN_PROGRESS,  // Partita in corso
    GAME_STATE_FINISHED      // Partita terminata, in attesa rematch
} GameState;

typedef enum {
    CELL_EMPTY = 0,  // Cella vuota
    CELL_X,          // Simbolo X (player1)
    CELL_O           // Simbolo O (player2)
} Cell;

typedef struct {
    int id;                           // ID univoco della partita
    GameState state;                  // Stato corrente
    Cell board[3][3];                 // Griglia di gioco 3x3
    
    // Giocatori
    int player1_fd;                   // FD del proprietario (X)
    int player2_fd;                   // FD dell'avversario (O)
    char player1_name[MAX_NAME_LEN];
    char player2_name[MAX_NAME_LEN];
    
    // Turno e vincitore
    int current_turn_fd;              // FD del giocatore di turno
    int winner_fd;                    // FD del vincitore (-1 = pareggio)
    
    // Sistema richieste join
    int pending_joiner_fd;            // FD del giocatore in attesa di accettazione
    char pending_joiner_name[MAX_NAME_LEN];
    
    // Sistema rematch
    RematchChoice player1_accepted_rematch;  // PENDING/YES/NO
    RematchChoice player2_accepted_rematch;
    
    // Proprietà
    int owner_fd;                     // FD del proprietario originale (immutabile)
    bool loser_ready_for_rematch;     // Flag per invito rematch in coda
    
    // Statistiche
    int moves_count;
    bool solo_mode;
} Game;
```

**Array globale**: `Game games[MAX_GAMES]` (MAX = 20)

**Valori speciali per FD**:
- `>= 0` : FD valido (giocatore connesso)
- `-1` : Nessun giocatore/mai connesso
- `-2` : Giocatore disconnesso temporaneamente (può rientrare)
- `-3` : Turno congelato in attesa del proprietario

### 3. Gestione del Turno

Il campo `current_turn_fd` può assumere questi valori:

| Valore | Significato |
|--------|-------------|
| `>= 0` | FD del giocatore di turno |
| `-1` | Nessun turno attivo |
| `-2` | Turno congelato: player2 disconnesso durante il suo turno |
| `-3` | Turno congelato: proprietario disconnesso durante il suo turno |

**Comportamento**:
- Quando un giocatore disconnesso rientra, il turno si **scongela** automaticamente
- Il giocatore connesso può completare il proprio turno, poi il turno si congela

---

## 🔌 Protocollo di Comunicazione

### Formato Messaggi

Tutti i messaggi sono **stringhe di testo** terminate da `\n`.

**Struttura**: `<TIPO>:<COMANDO> [parametri]\n`

Tipi di messaggi:
- `CMD:` - Comandi dal server al client
- `RESP:` - Risposte del server ai comandi del client
- `NOTIFY:` - Notifiche asincrone
- `ERROR:` - Messaggi di errore

### Comandi Client → Server

#### 1. Autenticazione

```
NAME <nome_giocatore>
```
**Descrizione**: Imposta il nome del giocatore (deve essere univoco)

**Risposte**:
- `RESP:NAME_OK` - Nome accettato → stato = LOBBY
- `ERROR:NAME_TAKEN` - Nome già in uso

---

#### 2. Gestione Lobby

##### `LIST`
Richiede la lista delle partite disponibili

**Risposta**:
```
RESP:GAMES_LIST;<id>,<proprietario>,<stato>[,<avversario>]|...
```

**Esempio**:
```
RESP:GAMES_LIST;1,Alice,Waiting|2,Bob,In Progress,Charlie
```

**Stati possibili**:
- `Waiting` - In attesa di giocatori
- `Waiting-Disconnected` - Proprietario disconnesso
- `In Progress` - Partita in corso
- `In Progress-Paused` - Giocatore disconnesso

##### `CREATE`
Crea una nuova partita

**Risposte**:
- `RESP:CREATED <game_id>` - Partita creata con successo
- `ERROR:Server pieno` - Nessuno slot disponibile

---

#### 3. Join e Richieste

##### `JOIN_REQUEST <game_id>`
Invia richiesta per unirsi a una partita

**Risposte**:
- `RESP:REQUEST_SENT <game_id>` - Richiesta inviata → stato = WAITING
- `ERROR:GAME_NOT_FOUND` - Partita non trovata
- `ERROR:ALREADY_PENDING` - C'è già una richiesta in sospeso

**Notifica al proprietario**:
```
NOTIFY:JOIN_REQUEST <nome_richiedente>
```

##### `ACCEPT <nome_giocatore>`
Accetta una richiesta di join (solo proprietario)

**Effetti**:
- Partita passa a `IN_PROGRESS`
- Entrambi i giocatori ricevono `NOTIFY:GAME_START`
- Inizia il broadcast dello stato della board

**Formato `NOTIFY:GAME_START`**:
```
NOTIFY:GAME_START <game_id> <simbolo> <nome_avversario>
```
- `<simbolo>` = `X` (proprietario) o `O` (avversario)

##### `REJECT <nome_giocatore>`
Rifiuta una richiesta di join (solo proprietario)

**Notifica al richiedente**:
```
RESP:JOIN_REJECTED <game_id> <nome_proprietario>
```

---

#### 4. Gameplay

##### `MOVE <riga> <colonna>`
Effettua una mossa (0-2 per riga/colonna)

**Prerequisiti**:
- Stato = `CLIENT_STATE_PLAYING`
- È il turno del giocatore (`current_turn_fd == player_fd`)
- Cella vuota

**Effetti**:
- Aggiorna la board
- Invia `NOTIFY:BOARD` a entrambi i giocatori
- Passa il turno all'avversario con `NOTIFY:YOUR_TURN`

**Controllo vittoria/pareggio**:
- **Vittoria**: Partita → `FINISHED`, invia `NOTIFY:GAMEOVER END_WIN/END_LOSE`
- **Pareggio**: Partita → `WAITING`, reset board, proprietario può accettare nuovi giocatori

**Errori**:
```
ERROR:Formato della mossa non valido. Usa: MOVE <riga> <colonna>
ERROR:Mossa non valida (fuori dai limiti 0-2)
ERROR:Mossa non valida (cella occupata)
ERROR:Non è il tuo turno
```

##### Board Format

La board viene inviata come stringa di 17 caratteri:

```
NOTIFY:BOARD X - O - X O - - X
```

Rappresenta:
```
X | - | O
---------
- | X | O
---------
- | - | X
```

---

#### 5. Quit e Rematch

##### `QUIT`
Esce dalla partita corrente

**Comportamento**:
- Da `LOBBY`: No-op, rimane connesso
- Da `WAITING`: Annulla richiesta join → torna a LOBBY
- Da `PLAYING`: Gestione intelligente (vedi sotto)

**Gestione uscite in PLAYING**:

| Chi esce | Stato partita | Comportamento |
|----------|---------------|---------------|
| Proprietario | WAITING (solo) | Player1_fd = -2 (disconnesso), partita MANTENUTA |
| Proprietario | IN_PROGRESS | Player1_fd = -2, turno congelato se era il suo turno |
| Player2 | IN_PROGRESS | Player2_fd = -2, turno congelato se era il suo turno |
| Vincitore | FINISHED | Partita ELIMINATA, perdente torna in LOBBY |
| Perdente | FINISHED | Partita mantenuta per il vincitore |

##### Sistema Rematch (Solo Vincitore)

**Flusso completo**:

1. **Fine partita**:
   ```
   NOTIFY:GAMEOVER END_WIN   (al vincitore)
   NOTIFY:GAMEOVER END_LOSE  (al perdente)
   ```

2. **Vincitore clicca OK** → Client invia:
   ```
   ACK_GAMEOVER
   ```

3. **Server invia popup scelta**:
   ```
   CMD:REMATCH_OFFER
   ```

4. **Vincitore risponde**:
   ```
   REMATCH YES  → Partita torna in WAITING, invita il perdente
   REMATCH NO   → Partita ELIMINATA, entrambi in LOBBY
   ```

5. **Perdente clicca OK** → Client invia:
   ```
   ACK_GAMEOVER_LOSER
   ```

6. **Se il vincitore ha già accettato**:
   ```
   NOTIFY:OPPONENT_ACCEPTED_REMATCH <game_id>
   ```
   → Perdente riceve popup per rientrare nella partita

**Timing intelligente**:
- Se il vincitore accetta **PRIMA** che il perdente clicchi OK → Invito salvato in coda
- Quando il perdente clicca OK → Invito mostrato immediatamente

---

### Comandi Server → Client

#### Notifiche di Board

```
NOTIFY:BOARD <board_string>
```
Inviato dopo ogni mossa valida

```
NOTIFY:YOUR_TURN
```
Inviato al giocatore di turno

#### Notifiche di Disconnessione

```
NOTIFY:OPPONENT_DISCONNECTED <messaggio>
```
L'avversario si è disconnesso, partita in pausa

```
NOTIFY:OWNER_RECONNECTED <messaggio>
```
Il proprietario è rientrato dopo disconnessione

```
NOTIFY:OPPONENT_RECONNECTED <messaggio>
```
L'avversario è rientrato dopo disconnessione

#### Fine Partita

```
NOTIFY:GAMEOVER END_WIN     # Hai vinto
NOTIFY:GAMEOVER END_LOSE    # Hai perso
NOTIFY:GAMEOVER DRAW        # Pareggio
```

#### Rematch

```
CMD:REMATCH_OFFER
```
Popup scelta rematch per il vincitore

```
NOTIFY:OPPONENT_ACCEPTED_REMATCH <game_id>
```
Il vincitore ha accettato il rematch, vuoi giocare di nuovo?

```
NOTIFY:REMATCH_DECLINED
```
Il vincitore ha rifiutato il rematch

---

## 🎮 Logica di Gioco

### Ciclo di Vita di una Partita

```
┌──────────────┐
│   EMPTY      │ (Slot libero)
└──────┬───────┘
       │ CREATE
       ▼
┌──────────────┐
│   WAITING    │ (Proprietario in attesa)
│              │ • Può giocare da solo
│              │ • Aspetta JOIN_REQUEST
└──────┬───────┘
       │ ACCEPT
       ▼
┌──────────────┐
│ IN_PROGRESS  │ (Partita attiva)
│              │ • Turni alternati
│              │ • Controllo vittoria
└──────┬───────┘
       │ WIN/DRAW
       ▼
┌──────────────┐
│  FINISHED    │ (Fine partita)
│              │ • Vincitore decide rematch
│              │ • Se YES → WAITING
│              │ • Se NO → EMPTY
└──────────────┘
```



```c
bool board_full(const Cell board[3][3])
```

Ritorna `true` se tutte le celle sono diverse da `CELL_EMPTY`.

**Complessità**: O(9) = O(1)

---

## 🔄 Gestione Connessioni

### Riconnessione Automatica

Il server mantiene lo stato della partita anche quando un giocatore si disconnette.

#### Identificazione Giocatore

**Chiave primaria**: `player_name` (univoco)

Quando un client si riconnette con lo stesso nome:
1. Il server cerca una partita con `player1_name == nome` o `player2_name == nome`
2. Se il giocatore era disconnesso (`player_fd == -2`), lo ripristina
3. Se il turno era congelato, lo scongela

#### Flusso Riconnessione

**Scenario**: Player2 disconnesso durante partita

```
1. Player2 disconnette
   → game->player2_fd = -2
   → Se era il suo turno: current_turn_fd = -2

2. Player1 continua a giocare
   → Dopo la sua mossa: turno si CONGELA

3. Player2 si riconnette
   → Invia: JOIN_REQUEST <game_id>
   → Server riconosce: player2_name == nome richiedente
   → game->player2_fd = <nuovo_fd>
   → Se turno congelato (-2): current_turn_fd = <nuovo_fd>
   → Invia: NOTIFY:OPPONENT_RECONNECTED al Player1
```


## 🏆 Sistema Rematch

### Meccanismo ACK a 2 Fasi

**Problema**: Evitare che il perdente riceva messaggi prima di aver visto il risultato.

**Soluzione**: Sistema acknowledgment con coda

#### Fase 1: Fine Partita

```
Server invia:
  - NOTIFY:GAMEOVER END_WIN (al vincitore)
  - NOTIFY:GAMEOVER END_LOSE (al perdente)

Client mostra popup:
  - Vincitore: "HAI VINTO! Clicca OK per decidere cosa fare"
  - Perdente: "Hai perso. Clicca OK per tornare alla lobby"
```

#### Fase 2: Conferma Vincitore

```
Vincitore clicca OK → Client invia: ACK_GAMEOVER

Server riceve ACK_GAMEOVER:
  - Invia: CMD:REMATCH_OFFER
  - Vincitore vede popup: "Vuoi rigiocare o tornare alla lobby?"
```

#### Fase 3: Conferma Perdente

```
Perdente clicca OK → Client invia: ACK_GAMEOVER_LOSER

Server riceve ACK_GAMEOVER_LOSER:
  - Imposta: game->loser_ready_for_rematch = true
  - Se il vincitore ha già accettato rematch:
      → Invia subito: NOTIFY:OPPONENT_ACCEPTED_REMATCH
```

#### Fase 4: Scelta Vincitore

```
Vincitore sceglie:

REMATCH YES:
  - game->state = GAME_STATE_WAITING
  - Reset board
  - game->player1_fd = vincitore (owner)
  - game->player2_fd = -1
  
  Se loser_ready_for_rematch == true:
    → Invia subito: NOTIFY:OPPONENT_ACCEPTED_REMATCH <game_id>
  Altrimenti:
    → Messaggio in coda (invio quando perdente clicca OK)

REMATCH NO:
  - Elimina partita: reset_game_slot_to_empty_unsafe()
  - Invia: NOTIFY:REMATCH_DECLINED al perdente
```

### Gestione Timing

**Caso A**: Perdente clicca OK PRIMA che il vincitore scelga
```
1. Perdente: ACK_GAMEOVER_LOSER → loser_ready = true
2. Vincitore: REMATCH YES → Invia subito NOTIFY:OPPONENT_ACCEPTED_REMATCH
```

**Caso B**: Vincitore sceglie PRIMA che il perdente clicchi OK
```
1. Vincitore: REMATCH YES → loser_ready = false → Nessun invio
2. Perdente: ACK_GAMEOVER_LOSER → loser_ready = true → Controlla se partita in WAITING
3. Se sì: Invia NOTIFY:OPPONENT_ACCEPTED_REMATCH
```

**Implementazione** (`process_ack_gameover_loser`):
```c
void process_ack_gameover_loser(int client_idx) {
    // ...
    games[game_idx].loser_ready_for_rematch = true;
    
    // Se la partita è già tornata in WAITING, il vincitore ha già accettato
    if (games[game_idx].state == GAME_STATE_WAITING) {
        // Invia invito rematch al perdente
        send_to_client(caller_fd, NOTIFY_OPPONENT_ACCEPTED_REMATCH);
    }
}
```

---

## 🔧 Compilazione ed Esecuzione

### Requisiti

- **GCC** o **Clang** (C99 o superiore)
- **pthread** library
- Sistema operativo: Linux / macOS

### Compilazione

```bash
cd tris_server

gcc -o tris_server \
    server.c \
    client_h.c \
    protocol.c \
    game_logic.c \
    utils.c \
    globals.c \
    -lpthread \
    -Wall \
    -Wextra
```

**Flag raccomandati**:
- `-lpthread` : Link libreria POSIX threads
- `-Wall -Wextra` : Abilita tutti i warning
- `-O2` : Ottimizzazione (opzionale)
- `-g` : Simboli di debug (opzionale)

### Esecuzione

```bash
./tris_server
```

**Output atteso**:
```
[timestamp] - Inizializzazione strutture dati server...
[timestamp] - Impostazione socket server...
[timestamp] - Server in ascolto sulla porta 12345... (Max Client Connessi: 20, Max Partite: 20)
```

### Configurazione

**Porta**: Modificare in `types.h`
```c
#define PORT 12345
```

**Limiti**:
```c
#define MAX_TOTAL_CLIENTS 20  // Max client simultanei
#define MAX_GAMES 20          // Max partite simultanee
#define BUFFER_SIZE 1024      // Dimensione buffer messaggi
```

### Terminazione

**Graceful shutdown**:
```bash
Ctrl+C  (SIGINT)
kill <pid>  (SIGTERM)
```

Il server:
1. Smette di accettare nuove connessioni
2. Notifica tutti i client: `NOTIFY:SERVER_SHUTDOWN`
3. Chiude tutte le socket
4. Attende terminazione thread (1 secondo)
5. Distrugge i mutex
6. Esce

---

### Gestione Errori

**SIGPIPE**: Ignorato (`SIG_IGN`) per evitare crash quando un client disconnette durante una `send()`

**EINTR**: Gestito nei loop di `accept()` e `read()`

### Thread Safety

**Pattern sicuro**:
```c
pthread_mutex_lock(&client_list_mutex);
pthread_mutex_lock(&game_list_mutex);

// ... operazioni critiche ...

pthread_mutex_unlock(&game_list_mutex);
pthread_mutex_unlock(&client_list_mutex);
```

**ATTENZIONE**: Rilasciare sempre i mutex nello stesso ordine inverso!

---

## 📚 Riferimenti

### Stati del Sistema

**Stati Client**:
- CONNECTED → LOBBY → WAITING → PLAYING → LOBBY

**Stati Partita**:
- EMPTY → WAITING → IN_PROGRESS → FINISHED → (WAITING o EMPTY)

---
