#!/bin/bash

##############################################
# Script per avviare 2 client Tris JavaFX
##############################################

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   Avvio Client Tris Multiplayer JavaFX    ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════╝${NC}"
echo ""

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$SCRIPT_DIR"
CLIENT_DIR="$PROJECT_DIR/tris_client"

SERVER_HOST="localhost"
SERVER_PORT="12345"

echo -e "${YELLOW} Verifico che il server Docker sia attivo...${NC}"
cd "$PROJECT_DIR"

if ! docker-compose ps | grep -q "tris_server.*Up"; then
    echo -e "${RED} ERRORE: Il server Docker non è in esecuzione!${NC}"
    echo -e "${YELLOW} Avvia il server con: docker-compose up -d tris_server${NC}"
    exit 1
fi

echo -e "${GREEN} Server Docker attivo e funzionante${NC}"
echo ""

if [ ! -d "$CLIENT_DIR/javafx-sdk-17.0.17" ]; then
    echo -e "${RED} ERRORE: JavaFX SDK non trovato${NC}"
    exit 1
fi

echo -e "${YELLOW}🔨 Compilazione client JavaFX...${NC}"
cd "$CLIENT_DIR"

javac --module-path javafx-sdk-17.0.17/lib --add-modules javafx.controls *.java

if [ $? -eq 0 ]; then
    echo -e "${GREEN} Compilazione completata${NC}"
else
    echo -e "${RED} Compilazione fallita${NC}"
    exit 1
fi

echo ""

start_client() {
    local CLIENT_NUM=$1
    local DELAY=$2
    
    echo -e "${BLUE}🎮 Avvio Client $CLIENT_NUM...${NC}"
    sleep $DELAY
    
    java --module-path javafx-sdk-17.0.17/lib \
         --add-modules javafx.controls \
         TrisClientFX "$SERVER_HOST" "$SERVER_PORT" > /dev/null 2>&1 &
    
    local PID=$!
    echo -e "${GREEN} Client $CLIENT_NUM avviato (PID: $PID)${NC}"
    echo $PID >> /tmp/tris_clients_pids.txt
}

rm -f /tmp/tris_clients_pids.txt

echo -e "${YELLOW} Avvio dei client...${NC}"
echo ""

start_client 1 0
start_client 2 1

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║      Due client avviati con successo!      ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════╝${NC}"
echo ""

sleep 2
ACTIVE_COUNT=0
if [ -f /tmp/tris_clients_pids.txt ]; then
    while read PID; do
        ps -p $PID > /dev/null 2>&1 && ((ACTIVE_COUNT++))
    done < /tmp/tris_clients_pids.txt
fi

echo -e "${GREEN} Client attivi: $ACTIVE_COUNT/2${NC}"
[ $ACTIVE_COUNT -lt 2 ] && echo -e "${YELLOW}  Verifica finestre JavaFX${NC}"
echo ""
