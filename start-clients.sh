#!/bin/bash

##############################################
# Script per avviare 2 client Tris JavaFX
##############################################

set -e

# Colori per output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   Avvio Client Tris Multiplayer JavaFX    ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════╝${NC}"
echo ""

# Directory del progetto
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$SCRIPT_DIR"
CLIENT_DIR="$PROJECT_DIR/tris_client"

# Parametri di connessione
SERVER_HOST="localhost"
SERVER_PORT="12345"

# Verifica che il server Docker sia in esecuzione
echo -e "${YELLOW}🔍 Verifico che il server Docker sia attivo...${NC}"
cd "$PROJECT_DIR"

if ! docker-compose ps | grep -q "tris_server.*Up"; then
    echo -e "${RED} ERRORE: Il server Docker non è in esecuzione!${NC}"
    echo -e "${YELLOW} Avvia il server con: docker-compose up -d tris_server${NC}"
    exit 1
fi

echo -e "${GREEN} Server Docker attivo e funzionante${NC}"
echo ""

# Verifica che JavaFX SDK esista
if [ ! -d "$CLIENT_DIR/javafx-sdk-17.0.17" ]; then
    echo -e "${RED} ERRORE: JavaFX SDK non trovato in $CLIENT_DIR/javafx-sdk-17.0.17${NC}"
    exit 1
fi

# Compilazione client (se necessario)
echo -e "${YELLOW}🔨 Compilazione client JavaFX...${NC}"
cd "$CLIENT_DIR"

javac --module-path javafx-sdk-17.0.17/lib \
      --add-modules javafx.controls \
      -d src \
      src/*.java 2>/dev/null

if [ $? -eq 0 ]; then
    echo -e "${GREEN} Compilazione completata${NC}"
else
    echo -e "${RED} ERRORE: Compilazione fallita${NC}"
    exit 1
fi

echo ""

# Funzione per avviare un client
start_client() {
    local CLIENT_NUM=$1
    local DELAY=$2
    
    echo -e "${BLUE} Avvio Client $CLIENT_NUM...${NC}"
    
    # Piccolo delay per evitare race conditions
    sleep $DELAY
    
    # Avvia client in background con output rediretto
    java -cp src \
         --module-path javafx-sdk-17.0.17/lib \
         --add-modules javafx.controls \
         TrisClientFX "$SERVER_HOST" "$SERVER_PORT" \
         > /dev/null 2>&1 &
    
    local PID=$!
    echo -e "${GREEN} Client $CLIENT_NUM avviato (PID: $PID)${NC}"
    
    # Salva PID in file temporaneo
    echo $PID >> /tmp/tris_clients_pids.txt
}

# Pulisci file PID precedente
rm -f /tmp/tris_clients_pids.txt

echo -e "${YELLOW}🚀 Avvio dei client...${NC}"
echo ""

# Avvia 2 client con un piccolo delay tra loro
start_client 1 0
start_client 2 1

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║      Due client avviati con successo!      ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE} Per fermare i client:${NC}"
echo -e "   ./stop-clients.sh"
echo ""

# Attendi qualche secondo per dare tempo ai client di avviarsi
sleep 2

# Verifica che i processi siano ancora attivi
ACTIVE_COUNT=0
if [ -f /tmp/tris_clients_pids.txt ]; then
    while read PID; do
        if ps -p $PID > /dev/null 2>&1; then
            ((ACTIVE_COUNT++))
        fi
    done < /tmp/tris_clients_pids.txt
fi

echo -e "${GREEN} Client attivi verificati: $ACTIVE_COUNT/2${NC}"

if [ $ACTIVE_COUNT -lt 2 ]; then
    echo -e "${YELLOW}   Attenzione: Non tutti i client sono attivi!${NC}"
    echo -e "${YELLOW}   Verifica che le finestre JavaFX si siano aperte correttamente.${NC}"
fi
