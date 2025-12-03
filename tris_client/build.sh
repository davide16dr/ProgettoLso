#!/bin/bash

# Script per compilare ed eseguire il client JavaFX

echo "╔════════════════════════════════════════╗"
echo "║   Build Client JavaFX Tris             ║"
echo "╚════════════════════════════════════════╝"

# Variabili
JAVAFX_VERSION="17.0.17"
JAVAFX_SDK_PATH="javafx-sdk-$JAVAFX_VERSION"
JAVAFX_LIB="$JAVAFX_SDK_PATH/lib"

# Controlla se JavaFX SDK esiste
if [ ! -d "$JAVAFX_SDK_PATH" ]; then
    echo "❌ JavaFX SDK non trovato in $JAVAFX_SDK_PATH"
    echo ""
    echo "Per scaricare JavaFX:"
    echo "1. Vai su https://gluonhq.com/products/javafx/"
    echo "2. Scarica JavaFX SDK $JAVAFX_VERSION per il tuo OS"
    echo "3. Estrai in questa directory come $JAVAFX_SDK_PATH"
    echo ""
    echo "Oppure usa il sistema JavaFX se installato:"
    echo "  javac --module-path \$PATH_TO_FX --add-modules javafx.controls *.java"
    echo "  java --module-path \$PATH_TO_FX --add-modules javafx.controls TrisClientFX"
    exit 1
fi

# Compila
echo "📦 Compilazione..."
javac --module-path "$JAVAFX_LIB" \
      --add-modules javafx.controls \
      *.java

if [ $? -eq 0 ]; then
    echo "✅ Compilazione completata!"
    echo ""
    echo "Per eseguire:"
    echo "  ./run.sh"
    echo ""
    echo "Oppure:"
    echo "  java --module-path $JAVAFX_LIB --add-modules javafx.controls TrisClientFX"
else
    echo "❌ Errore durante la compilazione"
    exit 1
fi