# Tris Multiplayer - Client-Server Game

Gioco del Tris multiplayer con server multi-thread in C e client grafico JavaFX.

---

## Prerequisiti

### Docker
```bash
# Docker Engine >= 20.10
docker --version

# Docker Compose >= 1.29
docker-compose --version
```

### Java & JavaFX (per client locale)
```bash
# JDK 17 o superiore
java -version

# JavaFX SDK 17.0.17
# Già incluso in: tris_client/javafx-sdk-17.0.17/
```

### Strumenti di Build
```bash
# GCC (per compilazione server locale)
gcc --version
```

---

## Start

### Avvia Server 
#### DOCKER
```bash
# Dalla directory principale
cd /<path>/ProgettoLso

# Build e avvio server in background
docker-compose up -d tris_server

# Verifica stato (deve essere "healthy")
docker-compose ps

# Visualizza log (opzionale)
docker-compose logs -f tris_server
```
#### SENZA DOCKER
```bash
cd tris_server

# Compila
gcc -o tris_server server.c client_h.c protocol.c \
    game_logic.c utils.c globals.c -lpthread -Wall -O2

# Esegui
./tris_server

# Stop: Ctrl+C
```

###  Avvia Client (Locale)
#### Opzione A: Comando Manuale
```bash
cd tris_client

# Compila
./build.sh

# Esegui
./run.sh
```

#### Opzione B: Script Automatico (2 client)
```bash
# Dalla directory principale
cd /<path>/ProgettoLso

# Avvia server + 2 client
./start-clients.sh
```

---

## Stop Sistema

```bash
# Ferma client (nel terminale dove è in esecuzione)
Ctrl+C 


# Ferma i client
./stop-clients.sh

# Ferma server Docker
cd /<path>/ProgettoLso
docker-compose down

# Ferma server Docker (nel terminale dove è in esecuzione)
Ctrl+C
```