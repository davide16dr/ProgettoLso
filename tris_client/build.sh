#!/bin/bash

# Script per compilare il client JavaFX

echo "╔════════════════════════════════════════╗"
echo "║   Build Client JavaFX Tris             ║"
echo "╚════════════════════════════════════════╝"

# Variabili
JAVAFX_VERSION="17.0.17"
JAVAFX_SDK_PATH="javafx-sdk-$JAVAFX_VERSION"
JAVAFX_LIB="$JAVAFX_SDK_PATH/lib"

# Controlla se JavaFX SDK esiste
if [ ! -d "$JAVAFX_SDK_PATH" ]; then
    echo " JavaFX SDK non trovato in $JAVAFX_SDK_PATH"
    echo ""
    exit 1
fi

# Compila
echo " Compilazione..."
javac --module-path "$JAVAFX_LIB" \
      --add-modules javafx.controls \
      *.java

if [ $? -eq 0 ]; then
    echo " Compilazione completata!"
    echo ""
    echo "Per eseguire:"
    echo "  ./run.sh"
    echo ""
else
    echo " Errore durante la compilazione"
    exit 1
fi