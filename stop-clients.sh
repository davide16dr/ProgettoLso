#!/bin/bash

##############################################
# Script per fermare i client Tris JavaFX
##############################################

# Colori per output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}╔════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     Arresto Client Tris JavaFX             ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════╝${NC}"
echo ""

PID_FILE="/tmp/tris_clients_pids.txt"

if [ ! -f "$PID_FILE" ]; then
    echo -e "${YELLOW}   Nessun file PID trovato.${NC}"
    echo -e "${YELLOW}   I client potrebbero essere già stati fermati.${NC}"
    exit 0
fi

echo -e "${YELLOW}🔍 Ricerca client attivi...${NC}"

STOPPED_COUNT=0
NOT_FOUND_COUNT=0

while read PID; do
    if ps -p $PID > /dev/null 2>&1; then
        echo -e "${BLUE} Arresto client con PID: $PID${NC}"
        kill $PID 2>/dev/null
        
        # Attendi che il processo termini
        sleep 1
        
        # Verifica se il processo è ancora attivo (force kill se necessario)
        if ps -p $PID > /dev/null 2>&1; then
            echo -e "${YELLOW}      Processo resistente, forzo terminazione...${NC}"
            kill -9 $PID 2>/dev/null
        fi
        
        echo -e "${GREEN} Client $PID fermato${NC}"
        ((STOPPED_COUNT++))
    else
        echo -e "${YELLOW} Client PID $PID già terminato${NC}"
        ((NOT_FOUND_COUNT++))
    fi
done < "$PID_FILE"

# Rimuovi file PID
rm -f "$PID_FILE"

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║           Operazione completata            ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════╝${NC}"
echo ""

if [ $STOPPED_COUNT -gt 0 ]; then
    echo -e "${GREEN} Tutti i client sono stati fermati correttamente${NC}"
else
    echo -e "${YELLOW}ℹ  Nessun client attivo da fermare${NC}"
fi

echo ""
