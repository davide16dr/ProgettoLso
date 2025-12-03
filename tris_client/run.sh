#!/bin/bash

# Script per eseguire il client JavaFX

echo "╔════════════════════════════════════════╗"
echo "║   Avvio Client JavaFX Tris             ║"
echo "╚════════════════════════════════════════╝"
echo ""

# Variabili
JAVAFX_VERSION="17.0.17"
JAVAFX_SDK_PATH="javafx-sdk-$JAVAFX_VERSION"
JAVAFX_LIB="$JAVAFX_SDK_PATH/lib"

# Controlla se è compilato
if [ ! -f "TrisClientFX.class" ]; then
    echo "❌ Applicazione non compilata. Esegui ./build.sh prima"
    exit 1
fi

# Controlla se JavaFX SDK esiste
if [ ! -d "$JAVAFX_SDK_PATH" ]; then
    echo "⚠️  JavaFX SDK non trovato, provo con installazione di sistema..."
    java --add-modules javafx.controls TrisClientFX
else
    # Esegui con JavaFX SDK locale
    java --module-path "$JAVAFX_LIB" \
         --add-modules javafx.controls \
         TrisClientFX
fi