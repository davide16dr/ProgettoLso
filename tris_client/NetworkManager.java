import javafx.application.Platform;
import java.io.*;
import java.net.Socket;

public class NetworkManager {
    private TrisClientFX client;
    private GameController gameController;
    private Socket socket;
    private BufferedReader reader;
    private PrintWriter writer;
    private Thread receiveThread;
    private boolean connected = false;
    private boolean pendingRematchOffer = false; 
    private boolean readyForRematchInvite = false; 
    private boolean waitingForRematchChoice = false; 
    private Integer pendingRematchGameId = null; 
    
    public NetworkManager(TrisClientFX client, GameController gameController) {
        this.client = client;
        this.gameController = gameController;
    }
    
    public void connect(String host, int port, String playerName) {
        try {
            socket = new Socket(host, port);
            reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            writer = new PrintWriter(socket.getOutputStream(), true);
            connected = true;
            
            System.out.println("✓ Connesso al server " + host + ":" + port);
            
            
            receiveThread = new Thread(this::receiveLoop);
            receiveThread.setDaemon(true);
            receiveThread.start();
            
            
            Thread.sleep(500);
            sendCommand("NAME " + playerName);
            
        } catch (Exception e) {
            e.printStackTrace();
            Platform.runLater(() -> {
                client.showAlert("Errore", "Impossibile connettersi al server: " + e.getMessage(),
                               javafx.scene.control.Alert.AlertType.ERROR);
            });
        }
    }
    
    public void sendCommand(String command) {
        System.out.println("🔥 DEBUG sendCommand chiamato con: " + command);
        System.out.println("🔥 DEBUG connected = " + connected);
        System.out.println("🔥 DEBUG writer = " + (writer != null ? "NOT NULL" : "NULL"));
        
        if (connected && writer != null) {
            writer.println(command);
            writer.flush(); 
            System.out.println("🔥 DEBUG Comando inviato e flushato: " + command);
            System.out.println(">>> " + command);
        } else {
            System.err.println("❌ ERRORE: Impossibile inviare comando! connected=" + connected + ", writer=" + (writer != null));
        }
    }
    
    private void receiveLoop() {
        try {
            String message;
            while (connected && (message = reader.readLine()) != null) {
                System.out.println("<<< " + message);
                handleMessage(message);
            }
        } catch (IOException e) {
            if (connected) {
                System.err.println("Errore ricezione: " + e.getMessage());
                Platform.runLater(() -> {
                    client.showAlert("Connessione Persa", 
                                   "La connessione al server è stata interrotta.",
                                   javafx.scene.control.Alert.AlertType.ERROR);
                });
            }
        } finally {
            disconnect();
        }
    }
    
     private void handleMessage(String message) {
        if (message == null || message.trim().isEmpty()) {
            return;
        }
        
        String msg = message.trim();
        System.out.println("DEBUG: Handling message: " + msg);
        
        
        if (msg.startsWith("CMD:GET_NAME")) {
            
        }
        
        else if (msg.startsWith("CMD:REMATCH_OFFER")) {
            if (isWaitingForRematchChoice()) {
                System.out.println("🏆 DEBUG: Vincitore ha ricevuto CMD:REMATCH_OFFER!");
                Platform.runLater(() -> {
                    client.showRematchChoice();
                });
            } else {
                System.out.println("🏆 DEBUG: Ignorato CMD:REMATCH_OFFER perché non in attesa.");
            }
        }
        
        
        else if (msg.startsWith("RESP:NAME_OK")) {
            client.showLoginSuccess();
        }
        else if (msg.startsWith("RESP:CREATED")) {
            String[] parts = msg.split("\\s+");
            if (parts.length >= 2) {
                int gameId = Integer.parseInt(parts[1]);
                System.out.println("DEBUG: Partita creata #" + gameId + ", chiamando showGameCreated()");
                client.showGameCreated(gameId);
            }
        }
        else if (msg.startsWith("RESP:GAMES_LIST")) {
            
            String[] parts = msg.split(";", 2);
            String gamesData = parts.length > 1 ? parts[1] : "";
            client.showGamesList(gamesData);
        }
        else if (msg.startsWith("RESP:REQUEST_SENT")) {
            
            client.addChatMessage("✓ Richiesta inviata");
        }
        else if (msg.startsWith("RESP:JOIN_ACCEPTED")) {
            
            String[] parts = msg.split("\\s+", 4);
            if (parts.length >= 4) {
                int gameId = Integer.parseInt(parts[1]);
                String symbol = parts[2];
                String opponent = parts[3];
                client.showGameStarted(gameId, symbol, opponent);
            }
        }
        else if (msg.startsWith("RESP:REJECT_OK")) {
            
            client.addChatMessage("✓ Giocatore rifiutato");
        }
        else if (msg.startsWith("RESP:JOIN_REJECTED")) {
            
            client.addChatMessage("✗ Richiesta rifiutata");
            client.showAlert("Rifiutato", "La tua richiesta è stata rifiutata",
                           javafx.scene.control.Alert.AlertType.INFORMATION);
        }
        else if (msg.startsWith("RESP:QUIT_OK")) {
            
            client.returnToLobby();
            client.addChatMessage("✓ Sei tornato alla lobby");
        }
        else if (msg.startsWith("RESP:REMATCH_ACCEPTED")) {
            
            String[] parts = msg.split("\\s+");
            if (parts.length >= 2) {
                int gameId = Integer.parseInt(parts[1]);
                client.showGameCreated(gameId); 
                client.addChatMessage("✓ Rematch accettato! Nuova partita #" + gameId);
            }
        }
        else if (msg.startsWith("RESP:REMATCH_DECLINED")) {
            
            client.returnToLobby();
            client.addChatMessage("✓ Tornato alla lobby");
        }
        
        
        else if (msg.startsWith("NOTIFY:JOIN_REQUEST")) {
            
            String[] parts = msg.split("\\s+", 2);
            if (parts.length >= 2) {
                String playerName = parts[1];
                client.showJoinRequest(playerName);
            }
        }
        else if (msg.startsWith("NOTIFY:GAME_START")) {
            
            String[] parts = msg.split("\\s+", 4);
            if (parts.length >= 4) {
                int gameId = Integer.parseInt(parts[1]);
                String symbol = parts[2];
                String opponent = parts[3];
                client.showGameStarted(gameId, symbol, opponent);
            }
        }
        else if (msg.startsWith("NOTIFY:REQUEST_CANCELLED")) {
            
            client.addChatMessage("⚠ Richiesta annullata");
        }
        else if (msg.startsWith("NOTIFY:BOARD")) {
            String boardData = msg.replace("NOTIFY:BOARD ", "");
            System.out.println("DEBUG: Aggiornamento board: " + boardData);
            client.updateBoard(boardData);
        }
        else if (msg.startsWith("NOTIFY:YOUR_TURN")) {
            System.out.println("DEBUG: È il tuo turno!");
            client.setYourTurn(true);
        }
        else if (msg.startsWith("NOTIFY:GAMEOVER")) {
            String result = msg.replace("NOTIFY:GAMEOVER ", "").trim();
            System.out.println("[NetworkManager] NOTIFY:GAMEOVER ricevuto: " + result);
            
            
            if ("END_WIN".equals(result)) {
                
                Platform.runLater(() -> {
                    javafx.scene.control.Alert endAlert = new javafx.scene.control.Alert(
                        javafx.scene.control.Alert.AlertType.INFORMATION);
                    endAlert.setTitle("🏆 Partita Terminata");
                    endAlert.setHeaderText("HAI VINTO!");
                    endAlert.setContentText("Complimenti! Hai vinto la partita.\nClicca OK per decidere cosa fare.");
                    endAlert.showAndWait();
                    
                    
                    System.out.println("✅ Vincitore ha cliccato OK, invio ACK_GAMEOVER al server");
                    sendCommand("ACK_GAMEOVER");
                    
                    
                    setWaitingForRematchChoice(true);
                });
            } else if ("END_LOSE".equals(result)) {
                
                Platform.runLater(() -> {
                    javafx.scene.control.Alert endAlert = new javafx.scene.control.Alert(
                        javafx.scene.control.Alert.AlertType.INFORMATION);
                    endAlert.setTitle("☹️ Partita Terminata");
                    endAlert.setHeaderText("Hai perso questa partita");
                    endAlert.setContentText("Non preoccuparti, puoi sempre giocare di nuovo!\nClicca OK per tornare alla lobby.");
                    endAlert.showAndWait();
                    
                    
                    System.out.println("✅ Perdente ha cliccato OK, invio ACK_GAMEOVER_LOSER al server");
                    sendCommand("ACK_GAMEOVER_LOSER");
                    
                    
                    client.returnToLobby();
                    client.addChatMessage("Tornato alla lobby. Attendi eventuale invito rematch...");
                    
                    
                    setReadyForRematchInvite(true);
                    
                    
                    new Thread(() -> {
                        try {
                            
                            Thread.sleep(300);
                            
                            
                            Platform.runLater(() -> {
                                if (pendingRematchGameId != null) {
                                    System.out.println("🎉 DEBUG: Invito rematch in coda trovato dopo delay! Mostro popup per partita #" + pendingRematchGameId);
                                    final Integer savedGameId = pendingRematchGameId;
                                    pendingRematchGameId = null; 
                                    
                                    
                                    System.out.println("🎯 DEBUG: Mostro showRematchInvite con gameId=" + savedGameId);
                                    client.showRematchInvite(savedGameId);
                                } else {
                                    System.out.println("ℹ️ DEBUG: Nessun invito rematch in coda dopo delay");
                                }
                            });
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }).start();
                });
            } else if ("DRAW".equals(result)) {
                
                Platform.runLater(() -> {
                    javafx.scene.control.Alert drawAlert = new javafx.scene.control.Alert(
                        javafx.scene.control.Alert.AlertType.INFORMATION);
                    drawAlert.setTitle("🤝 Partita Terminata");
                    drawAlert.setHeaderText("PAREGGIO!");
                    drawAlert.setContentText("La partita è finita in pareggio.\nClicca OK per tornare alla lobby.");
                    drawAlert.showAndWait();
                    
                    
                    client.returnToLobby();
                    client.addChatMessage("Partita finita in pareggio. Tornato alla lobby.");
                });
            } else {
                
                client.showGameOver(result);
            }
            client.setYourTurn(false);
        }
        else if (msg.startsWith("NOTIFY:OPPONENT_LEFT") ||
                 msg.startsWith("NOTIFY:WINNER_LEFT")) {
            
            client.addChatMessage("⚠ " + msg.replace("NOTIFY:", "").replace("_", " "));
            client.returnToLobby();
        }
        else if (msg.startsWith("NOTIFY:OPPONENT_DISCONNECTED")) {
            
            String disconnectMsg = msg.replace("NOTIFY:OPPONENT_DISCONNECTED ", "");
            client.addChatMessage("⚠ " + disconnectMsg);
            client.showOpponentDisconnected();
        }
        else if (msg.startsWith("NOTIFY:OPPONENT_ACCEPTED_REMATCH")) {
            
            System.out.println("🔥 DEBUG: NOTIFY:OPPONENT_ACCEPTED_REMATCH ricevuto!");
            System.out.println("🔥 DEBUG: Messaggio completo: " + msg);
            String[] parts = msg.split("\\s+");
            System.out.println("🔥 DEBUG: Numero di parti dopo split: " + parts.length);
            for (int i = 0; i < parts.length; i++) {
                System.out.println("🔥 DEBUG: parts[" + i + "] = '" + parts[i] + "'");
            }
            
            if (parts.length >= 2) {
                try {
                    int gameId = Integer.parseInt(parts[1]);
                    System.out.println("🔥 DEBUG: Perdente ricevuto invito rematch per partita #" + gameId);
                    System.out.println("🔥 DEBUG: readyForRematchInvite = " + readyForRematchInvite);
                    
                    
                    if (!readyForRematchInvite) {
                        System.out.println("⏸️ DEBUG: Perdente NON ancora pronto, salvo invito in coda");
                        pendingRematchGameId = gameId;
                    } else {
                        
                        System.out.println("✅ DEBUG: Perdente pronto, mostro popup subito");
                        Platform.runLater(() -> {
                            System.out.println("🎯 DEBUG: Sto per mostrare showRematchInvite con gameId=" + gameId);
                            client.showRematchInvite(gameId);
                        });
                    }
                } catch (NumberFormatException e) {
                    System.err.println("❌ ERRORE: Impossibile parsare gameId da: " + parts[1]);
                    e.printStackTrace();
                }
            } else {
                System.err.println("❌ ERRORE: NOTIFY:OPPONENT_ACCEPTED_REMATCH ha solo " + parts.length + " parti!");
            }
        }
        else if (msg.startsWith("NOTIFY:OPPONENT_DECLINED")) {
            
            client.addChatMessage("⚠ " + msg.replace("NOTIFY:", "").replace("_", " "));
            client.returnToLobby();
        }
        else if (msg.startsWith("NOTIFY:SERVER_SHUTDOWN")) {
            Platform.runLater(() -> {
                client.showAlert("Server", "Il server sta chiudendo...",
                               javafx.scene.control.Alert.AlertType.WARNING);
            });
            disconnect();
        }
        
        
        else if (msg.startsWith("ERROR:")) {
            String error = msg.replace("ERROR:", "");
            client.addChatMessage("✗ " + error);
            
            if (error.contains("NAME_TAKEN")) {
                Platform.runLater(() -> {
                    client.showAlert("Nome non disponibile", 
                                   "Questo nome è già in uso. Riprova con un altro nome.",
                                   javafx.scene.control.Alert.AlertType.ERROR);
                });
                disconnect();
            }
            else if (error.contains("Partita non disponibile") || 
                     error.contains("Partita piena") ||
                     error.contains("Partita non trovata")) {
                
                Platform.runLater(() -> {
                    client.showAlert("Impossibile Unirsi", error,
                                   javafx.scene.control.Alert.AlertType.WARNING);
                });
            }
        }
        
        else {
            
            client.addChatMessage("[Server]: " + msg);
        }
    }
    
    public boolean hasPendingRematchOffer() {
        return pendingRematchOffer;
    }
    
    public void clearPendingRematchOffer() {
        pendingRematchOffer = false;
    }
    
    public void setReadyForRematchInvite(boolean ready) {
        this.readyForRematchInvite = ready;
        System.out.println("🎯 setReadyForRematchInvite: " + ready);
    }
    
    public boolean isReadyForRematchInvite() {
        return readyForRematchInvite;
    }
    
    public void setWaitingForRematchChoice(boolean waiting) {
        this.waitingForRematchChoice = waiting;
        System.out.println("🎯 setWaitingForRematchChoice: " + waiting);
    }
    
    public boolean isWaitingForRematchChoice() {
        return waitingForRematchChoice;
    }
    
    public void disconnect() {
        connected = false;
        try {
            if (socket != null && !socket.isClosed()) {
                socket.close();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
    
    public boolean isConnected() {
        return connected;
    }
}