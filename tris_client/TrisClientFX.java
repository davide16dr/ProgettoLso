
import javafx.application.Application;
import javafx.application.Platform;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;
import javafx.scene.text.FontWeight;
import javafx.stage.Stage;

public class TrisClientFX extends Application {
    
    private NetworkManager networkManager;
    private GameController gameController;
    private UIManager uiManager;
    
    private Stage primaryStage;
    private Scene loginScene;
    private Scene lobbyScene;
    private Scene gameScene;
    
    private TextField nameField;
    private TextArea chatArea;
    private ListView<String> gamesList;
    private GridPane boardGrid;
    private Label statusLabel;
    private Label turnLabel;
    private Button[][] cellButtons;
    private Button refreshBtn;
    
    private String playerName;
    private String playerSymbol;
    private int currentGameId;
    private boolean canMove = false;
    private boolean isSoloMode = false; 
    
    @Override
    public void start(Stage stage) {
        this.primaryStage = stage;
        primaryStage.setTitle("Tris Client");
        
        
        gameController = new GameController(this);
        networkManager = new NetworkManager(this, gameController);
        uiManager = new UIManager(this);
        
        
        loginScene = createLoginScene();
        lobbyScene = createLobbyScene();
        gameScene = createGameScene();
        
        
        primaryStage.setScene(loginScene);
        primaryStage.setWidth(900);
        primaryStage.setHeight(700);
        primaryStage.show();
        
        
        primaryStage.setOnCloseRequest(e -> {
            if (networkManager != null) {
                networkManager.disconnect();
            }
            Platform.exit();
        });
    }
    
    private Scene createLoginScene() {
        VBox root = new VBox(20);
        root.setAlignment(Pos.CENTER);
        root.setPadding(new Insets(40));
        root.setStyle("-fx-background-color: #2C3E50;");
        
        Label titleLabel = new Label("TRIS ONLINE");
        titleLabel.setFont(Font.font("Arial", FontWeight.BOLD, 48));
        titleLabel.setTextFill(Color.web("#ECF0F1"));
        
        Label subtitleLabel = new Label("Connetti al server");
        subtitleLabel.setFont(Font.font("Arial", 18));
        subtitleLabel.setTextFill(Color.web("#BDC3C7"));
        
        
        HBox serverBox = new HBox(10);
        serverBox.setAlignment(Pos.CENTER);
        Label serverLabel = new Label("Server:");
        serverLabel.setTextFill(Color.web("#ECF0F1"));
        TextField serverField = new TextField("localhost");
        serverField.setPrefWidth(200);
        
        Label portLabel = new Label("Porta:");
        portLabel.setTextFill(Color.web("#ECF0F1"));
        TextField portField = new TextField("12345");
        portField.setPrefWidth(80);
        
        serverBox.getChildren().addAll(serverLabel, serverField, portLabel, portField);
        
        
        HBox nameBox = new HBox(10);
        nameBox.setAlignment(Pos.CENTER);
        Label nameLabel = new Label("Nome:");
        nameLabel.setTextFill(Color.web("#ECF0F1"));
        nameField = new TextField();
        nameField.setPromptText("Inserisci il tuo nome");
        nameField.setPrefWidth(300);
        nameBox.getChildren().addAll(nameLabel, nameField);
        
        
        Button connectBtn = new Button("CONNETTI");
        connectBtn.setStyle("-fx-background-color: #27AE60; -fx-text-fill: white; " +
                           "-fx-font-size: 16px; -fx-font-weight: bold; " +
                           "-fx-padding: 10 30; -fx-cursor: hand;");
        connectBtn.setOnAction(e -> {
            String name = nameField.getText().trim();
            if (name.isEmpty()) {
                showAlert("Errore", "Inserisci un nome!", Alert.AlertType.ERROR);
                return;
            }
            
            String host = serverField.getText().trim();
            int port = Integer.parseInt(portField.getText().trim());
            
            playerName = name;
            networkManager.connect(host, port, name);
        });
        
        
        nameField.setOnAction(e -> connectBtn.fire());
        
        Label statusLoginLabel = new Label("");
        statusLoginLabel.setTextFill(Color.web("#E74C3C"));
        
        root.getChildren().addAll(titleLabel, subtitleLabel, 
                                 new Label(" "), serverBox, nameBox, 
                                 connectBtn, statusLoginLabel);
        
        return new Scene(root);
    }
    
    private Scene createLobbyScene() {
        BorderPane root = new BorderPane();
        root.setStyle("-fx-background-color: #34495E;");
        
        
        VBox header = new VBox(10);
        header.setPadding(new Insets(20));
        header.setStyle("-fx-background-color: #2C3E50;");
        
        Label welcomeLabel = new Label("Benvenuto!");
        welcomeLabel.setId("welcomeLabel"); 
        welcomeLabel.setFont(Font.font("Arial", FontWeight.BOLD, 24));
        welcomeLabel.setTextFill(Color.web("#ECF0F1"));
        
        statusLabel = new Label("In lobby");
        statusLabel.setFont(Font.font("Arial", 14));
        statusLabel.setTextFill(Color.web("#95A5A6"));
        
        header.getChildren().addAll(welcomeLabel, statusLabel);
        root.setTop(header);
        
        
        VBox center = new VBox(15);
        center.setPadding(new Insets(20));
        
        Label gamesLabel = new Label("PARTITE DISPONIBILI");
        gamesLabel.setFont(Font.font("Arial", FontWeight.BOLD, 18));
        gamesLabel.setTextFill(Color.web("#ECF0F1"));
        
        gamesList = new ListView<>();
        gamesList.setPrefHeight(400);
        gamesList.setStyle("-fx-background-color: #2C3E50; " +
                          "-fx-control-inner-background: #2C3E50;");
        
        
        
        HBox gamesButtons = new HBox(10);
        gamesButtons.setAlignment(Pos.CENTER);
        
        refreshBtn = new Button("🔄 Aggiorna");
        Button joinBtn = new Button("➜ Unisciti");
        Button createBtn = new Button("✨ Crea Partita");
        
        styleButton(refreshBtn, "#3498DB");
        styleButton(joinBtn, "#27AE60");
        styleButton(createBtn, "#E67E22");
        
        refreshBtn.setOnAction(e -> networkManager.sendCommand("LIST"));
        joinBtn.setOnAction(e -> joinSelectedGame());
        createBtn.setOnAction(e -> createGame());
        
        gamesButtons.getChildren().addAll(refreshBtn, joinBtn, createBtn);
        
        center.getChildren().addAll(gamesLabel, gamesList, gamesButtons);
        root.setCenter(center);
        
        
        VBox rightPanel = new VBox(10);
        rightPanel.setPadding(new Insets(20));
        rightPanel.setPrefWidth(300);
        rightPanel.setStyle("-fx-background-color: #2C3E50;");
        
        Label chatLabel = new Label("MESSAGGI");
        chatLabel.setFont(Font.font("Arial", FontWeight.BOLD, 16));
        chatLabel.setTextFill(Color.web("#ECF0F1"));
        
        chatArea = new TextArea();
        chatArea.setEditable(false);
        chatArea.setWrapText(true);
        chatArea.setPrefHeight(500);
        chatArea.setStyle("-fx-control-inner-background: #34495E; " +
                         "-fx-text-fill: #ECF0F1;");
        
        rightPanel.getChildren().addAll(chatLabel, chatArea);
        root.setRight(rightPanel);
        
        return new Scene(root);
    }

    public void updateWelcomeLabel(String name) {
        Platform.runLater(() -> {
            for (var node : ((VBox) ((BorderPane) lobbyScene.getRoot()).getTop()).getChildren()) {
                if (node instanceof Label) {
                    Label lbl = (Label) node;
                    if (lbl.getFont().getSize() == 24.0) { 
                        lbl.setText("Benvenuto, " + name + "!");
                        break;
                    }
                }
            }
        });
    }
    
    private Scene createGameScene() {
        BorderPane root = new BorderPane();
        root.setStyle("-fx-background-color: #34495E;");
        
        
        VBox header = new VBox(10);
        header.setPadding(new Insets(20));
        header.setStyle("-fx-background-color: #2C3E50;");
        header.setAlignment(Pos.CENTER);
        
        Label gameLabel = new Label("PARTITA IN CORSO");
        gameLabel.setFont(Font.font("Arial", FontWeight.BOLD, 24));
        gameLabel.setTextFill(Color.web("#ECF0F1"));
        
        turnLabel = new Label("In attesa...");
        turnLabel.setFont(Font.font("Arial", FontWeight.BOLD, 18));
        turnLabel.setTextFill(Color.web("#F39C12"));
        
        header.getChildren().addAll(gameLabel, turnLabel);
        root.setTop(header);
        
        
        VBox center = new VBox(20);
        center.setAlignment(Pos.CENTER);
        center.setPadding(new Insets(40));
        
        boardGrid = new GridPane();
        boardGrid.setAlignment(Pos.CENTER);
        boardGrid.setHgap(10);
        boardGrid.setVgap(10);
        
        cellButtons = new Button[3][3];
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                Button btn = new Button("");
                btn.setPrefSize(120, 120);
                btn.setFont(Font.font("Arial", FontWeight.BOLD, 48));
                btn.setStyle("-fx-background-color: #2C3E50; " +
                           "-fx-text-fill: #ECF0F1; " +
                           "-fx-border-color: #7F8C8D; " +
                           "-fx-border-width: 2; " +
                           "-fx-cursor: hand;");
                
                final int r = row;
                final int c = col;
                btn.setOnAction(e -> makeMove(r, c));
                
                btn.setOnMouseEntered(e -> {
                    if (btn.getText().isEmpty() && canMove) {
                        btn.setStyle("-fx-background-color: #34495E; " +
                                   "-fx-text-fill: #ECF0F1; " +
                                   "-fx-border-color: #3498DB; " +
                                   "-fx-border-width: 3; " +
                                   "-fx-cursor: hand;");
                    }
                });
                
                btn.setOnMouseExited(e -> {
                    if (btn.getText().isEmpty()) {
                        btn.setStyle("-fx-background-color: #2C3E50; " +
                                   "-fx-text-fill: #ECF0F1; " +
                                   "-fx-border-color: #7F8C8D; " +
                                   "-fx-border-width: 2; " +
                                   "-fx-cursor: hand;");
                    }
                });
                
                cellButtons[row][col] = btn;
                boardGrid.add(btn, col, row);
            }
        }
        
        center.getChildren().add(boardGrid);
        root.setCenter(center);
        
        
        HBox bottom = new HBox(20);
        bottom.setAlignment(Pos.CENTER);
        bottom.setPadding(new Insets(20));
        
        Button quitBtn = new Button("Esci alla Lobby");
        styleButton(quitBtn, "#E74C3C");
        quitBtn.setOnAction(e -> quitGame());
        
        bottom.getChildren().add(quitBtn);
        root.setBottom(bottom);
        
        return new Scene(root);
    }
    
    private void styleButton(Button btn, String color) {
        btn.setStyle("-fx-background-color: " + color + "; " +
                    "-fx-text-fill: white; " +
                    "-fx-font-size: 14px; " +
                    "-fx-font-weight: bold; " +
                    "-fx-padding: 10 20; " +
                    "-fx-cursor: hand;");
    }
    
    private void createGame() {
        networkManager.sendCommand("CREATE");
        addChatMessage("Creazione partita...");
    }
    
    private void joinSelectedGame() {
        String selected = gamesList.getSelectionModel().getSelectedItem();
        if (selected == null) {
            showAlert("Errore", "Seleziona una partita!", Alert.AlertType.WARNING);
            return;
        }
        
        
        try {
            String gameId = selected.split("#")[1].split(" ")[0];
            
            networkManager.sendCommand("JOIN_REQUEST " + gameId);
            addChatMessage("Accesso a partita #" + gameId + "...");
        } catch (Exception e) {
            showAlert("Errore", "Formato partita non valido!", Alert.AlertType.ERROR);
        }
    }
    
    private void makeMove(int row, int col) {
        if (cellButtons[row][col].getText().isEmpty() && canMove) {
            cellButtons[row][col].setDisable(true);
            canMove = false; 
            turnLabel.setText("⏳ Mossa inviata...");
            turnLabel.setTextFill(Color.web("#F39C12"));

            networkManager.sendCommand("MOVE " + row + " " + col);
            
        }
    }
    
    private void quitGame() {
        
        Platform.runLater(() -> {
            addChatMessage("Uscita dalla partita...");
            returnToLobby(); 
        });
        networkManager.sendCommand("QUIT"); 
    }
    
    
    public void showLoginSuccess() {
        Platform.runLater(() -> {
            updateWelcomeLabel(playerName);
            primaryStage.setScene(lobbyScene);
            addChatMessage("✓ Connesso con successo!");
            
        });
    }
    
    public void showGamesList(String gamesData) {
        Platform.runLater(() -> {
            gamesList.getItems().clear();
            
            if (gamesData == null || gamesData.isEmpty()) {
                gamesList.getItems().add("Nessuna partita disponibile");
                return;
            }
            
            String[] games = gamesData.split("\\|");
            for (String game : games) {
                if (game.isEmpty()) continue;
                
                String[] info = game.split(",");
                if (info.length >= 3) {
                    String id = info[0];
                    String creator = info[1];
                    String state = info[2];
                    String opponent = info.length > 3 ? info[3] : "";
                    
                    String display;
                    
                    
                    if (state.equals("Waiting")) {
                        
                        display = String.format("#%s - %s (In attesa) 👤", id, creator);
                    } 
                    else if (state.equals("Waiting-Disconnected")) {
                        
                        if (!opponent.isEmpty()) {
                            display = String.format("#%s - %s (In attesa - %s uscito) 👤⚠️", id, creator, opponent);
                        } else {
                            display = String.format("#%s - %s (In attesa) 👤", id, creator);
                        }
                    }
                    else if (state.equals("In Progress")) {
                        
                        if (!opponent.isEmpty()) {
                            display = String.format("#%s - %s vs %s (In corso) ⚔️", id, creator, opponent);
                        } else {
                            display = String.format("#%s - %s (In corso) ⚔️", id, creator);
                        }
                    }
                    else if (state.equals("In Progress-Paused")) {
                        
                        if (!opponent.isEmpty()) {
                            display = String.format("#%s - %s vs %s (In pausa) ⏸️", id, creator, opponent);
                        } else {
                            display = String.format("#%s - %s (In pausa) ⏸️", id, creator);
                        }
                    }
                    else if (state.equals("Finished")) {
                        
                        if (!opponent.isEmpty()) {
                            display = String.format("#%s - %s vs %s (Terminata) ✓", id, creator, opponent);
                        } else {
                            display = String.format("#%s - %s (Terminata) ✓", id, creator);
                        }
                    }
                    else {
                        
                        display = String.format("#%s - %s (%s)", id, creator, state);
                    }
                    
                    gamesList.getItems().add(display);
                }
            }
        });
    }
    
    public void showGameCreated(int gameId) {
        Platform.runLater(() -> {
            System.out.println("DEBUG: showGameCreated chiamato per partita #" + gameId);
            currentGameId = gameId;
            
            
            addChatMessage("✓ Partita #" + gameId + " creata! Attendi richieste di join.");
            statusLabel.setText("Partita #" + gameId + " creata - In attesa di giocatori");
            
            
            System.out.println("DEBUG: Creatore rimane in LOBBY");
        });
    }
    
    public void showGameStarted(int gameId, String symbol, String opponent) {
        Platform.runLater(() -> {
            currentGameId = gameId;
            playerSymbol = symbol;
            isSoloMode = false; 
            primaryStage.setScene(gameScene);
            turnLabel.setText("Contro: " + opponent + " | Sei: " + symbol);
            turnLabel.setTextFill(Color.web("#F39C12"));
            clearBoard();
            addChatMessage("🎮 Partita iniziata contro " + opponent);
        });
    }
    
    public void updateBoard(String boardData) {
        Platform.runLater(() -> {
            String[] cells = boardData.trim().split("\\s+");
            if (cells.length == 9) {
                for (int i = 0; i < 9; i++) {
                    int row = i / 3;
                    int col = i % 3;
                    String symbol = cells[i].equals("-") ? "" : cells[i];
                    cellButtons[row][col].setText(symbol);
                    
                    if (!symbol.isEmpty()) {
                        cellButtons[row][col].setDisable(true);
                        if (symbol.equals("X")) {
                            cellButtons[row][col].setTextFill(Color.web("#3498DB"));
                        } else {
                            cellButtons[row][col].setTextFill(Color.web("#E74C3C"));
                        }
                    } else {
                        cellButtons[row][col].setDisable(false);
                    }
                }
            }
        });
    }
    
    public void setYourTurn(boolean isTurn) {
        Platform.runLater(() -> {
            canMove = isTurn;
            if (isTurn) {
                if (isSoloMode) {
                    turnLabel.setText("🎮 Modalità Solo - Puoi fare mosse!");
                    turnLabel.setTextFill(Color.web("#27AE60"));
                } else {
                    turnLabel.setText("♟ È IL TUO TURNO!");
                    turnLabel.setTextFill(Color.web("#27AE60"));
                }
            } else {
                
                if (!isSoloMode) {
                    turnLabel.setText("⏳ È il turno dell'avversario");
                    turnLabel.setTextFill(Color.web("#F39C12"));
                } else {
                    turnLabel.setText("⏳ In attesa di un avversario...");
                    turnLabel.setTextFill(Color.web("#95A5A6"));
                }
            }
        });
    }
    
    public void showGameOver(String result) {
        Platform.runLater(() -> {
            canMove = false;
            
            
            Alert gameEndAlert = new Alert(Alert.AlertType.INFORMATION);
            gameEndAlert.setTitle("Partita Terminata");
            gameEndAlert.setHeaderText("La partita è terminata!");
            gameEndAlert.setContentText("Clicca OK per vedere il risultato.");
            
            
            gameEndAlert.showAndWait();
            
            
            if (result.equals("WIN")) {
                
                Alert winAlert = new Alert(Alert.AlertType.CONFIRMATION);
                winAlert.setTitle("🏆 HAI VINTO!");
                winAlert.setHeaderText("Congratulazioni, hai vinto la partita!");
                winAlert.setContentText("Cosa vuoi fare?");
                
                ButtonType rematchBtn = new ButtonType("🎮 Rigiocare");
                ButtonType lobbyBtn = new ButtonType("🚪 Torna alla Lobby");
                winAlert.getButtonTypes().setAll(rematchBtn, lobbyBtn);
                
                winAlert.showAndWait().ifPresent(response -> {
                    if (response == rematchBtn) {
                        System.out.println("✅ Vincitore ha scelto: REMATCH YES");
                        networkManager.sendCommand("REMATCH YES");
                        addChatMessage("✅ Rimani in attesa di un avversario per il rematch...");
                        turnLabel.setText("⏳ In attesa di un nuovo avversario...");
                        turnLabel.setTextFill(Color.web("#F39C12"));
                        clearBoard();
                    } else {
                        System.out.println("❌ Vincitore ha scelto: REMATCH NO");
                        networkManager.sendCommand("REMATCH NO");
                        addChatMessage("❌ Partita eliminata - Tornando alla lobby...");
                        returnToLobby();
                    }
                });
            } else if (result.equals("LOSE")) {
                
                Alert loseAlert = new Alert(Alert.AlertType.INFORMATION);
                loseAlert.setTitle("Partita Persa");
                loseAlert.setHeaderText("☹ Hai perso questa partita");
                loseAlert.setContentText("Non preoccuparti, puoi sempre giocare di nuovo!");
                
                loseAlert.showAndWait();
                
                
                
                returnToLobby();
                addChatMessage("Tornato alla lobby. Attendi eventuale invito rematch...");
                
                
                networkManager.setReadyForRematchInvite(true);
                
            } else if (result.equals("DRAW")) {
                
                Alert drawAlert = new Alert(Alert.AlertType.INFORMATION);
                drawAlert.setTitle("Pareggio");
                drawAlert.setHeaderText("🤝 PAREGGIO!");
                drawAlert.setContentText("La partita è terminata in parità.");
                
                drawAlert.showAndWait();
                
                returnToLobby();
                addChatMessage("Pareggio! Tornato alla lobby.");
            }
        });
    }

    public void showRematchOffer() {
        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.CONFIRMATION);
            alert.setTitle("Rematch");
            alert.setHeaderText("Vuoi fare un rematch?");
            alert.setContentText("Scegli la tua risposta:");
            
            ButtonType yesBtn = new ButtonType("Sì");
            ButtonType noBtn = new ButtonType("No");
            alert.getButtonTypes().setAll(yesBtn, noBtn);
            
            alert.showAndWait().ifPresent(response -> {
                if (response == yesBtn) {
                    networkManager.sendCommand("REMATCH YES");
                    addChatMessage("✓ Rematch accettato - Nuova partita in attesa...");
                } else {
                    networkManager.sendCommand("REMATCH NO");
                    addChatMessage("✗ Rematch rifiutato - Tornando alla lobby...");
                }
            });
        });
    }
    
    public void showRematchChoice() {
        Platform.runLater(() -> {
            System.out.println("🏆 showRematchChoice() chiamato - Mostrando pop-up al vincitore!");
            
            Alert alert = new Alert(Alert.AlertType.CONFIRMATION);
            alert.setTitle("🏆 HAI VINTO!");
            alert.setHeaderText("Vuoi rigiocare oppure tornare in lobby?");
            alert.setContentText("Scegli cosa fare:");
            
            ButtonType rematchBtn = new ButtonType("🎮 Rigiocare");
            ButtonType lobbyBtn = new ButtonType("🚪 Lobby");
            alert.getButtonTypes().setAll(rematchBtn, lobbyBtn);
            
            alert.showAndWait().ifPresent(response -> {
                if (response == rematchBtn) {
                    System.out.println("✅ Vincitore ha scelto: REMATCH YES");
                    networkManager.sendCommand("REMATCH YES");
                    addChatMessage("✅ Rimani in attesa di un avversario per il rematch...");
                    turnLabel.setText("⏳ In attesa di un nuovo avversario...");
                    turnLabel.setTextFill(Color.web("#F39C12"));
                } else {
                    System.out.println("❌ Vincitore ha scelto: REMATCH NO");
                    networkManager.sendCommand("REMATCH NO");
                    addChatMessage("❌ Partita eliminata - Tornando alla lobby...");
                    returnToLobby();
                }
            });
        });
    }
    
    public void showJoinRequest(String playerName) {
        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.CONFIRMATION);
            alert.setTitle("Richiesta di Join");
            alert.setHeaderText(playerName + " vuole unirsi alla tua partita!");
            alert.setContentText("Accetti?");
            
            ButtonType acceptBtn = new ButtonType("✓ Accetta");
            ButtonType rejectBtn = new ButtonType("✗ Rifiuta");
            alert.getButtonTypes().setAll(acceptBtn, rejectBtn);
            
            alert.showAndWait().ifPresent(response -> {
                if (response == acceptBtn) {
                    networkManager.sendCommand("ACCEPT " + playerName);
                    addChatMessage("✓ Hai accettato " + playerName);
                } else {
                    networkManager.sendCommand("REJECT " + playerName);
                    addChatMessage("✗ Hai rifiutato " + playerName);
                }
            });
        });
    }
    
    public void showOpponentDisconnected() {
        Platform.runLater(() -> {
            turnLabel.setText("⚠️ Avversario disconnesso - In attesa...");
            turnLabel.setTextFill(Color.web("#E67E22"));
            
            
            
            
            if (canMove) {
                
                turnLabel.setText("⚠️ Avversario disconnesso - Puoi fare la tua mossa");
                turnLabel.setTextFill(Color.web("#27AE60"));
            }
            
        });
    }
    
    public void showRematchInvite(int gameId) {
        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.CONFIRMATION);
            alert.setTitle("Invito Rematch");
            alert.setHeaderText("Il vincitore vuole la rivincita!");
            alert.setContentText("Vuoi unirti alla partita #" + gameId + "?");
            
            ButtonType yesBtn = new ButtonType("Sì, gioca!");
            ButtonType noBtn = new ButtonType("No, grazie");
            alert.getButtonTypes().setAll(yesBtn, noBtn);
            
            alert.showAndWait().ifPresent(response -> {
                if (response == yesBtn) {
                    
                    networkManager.sendCommand("JOIN_REQUEST " + gameId);
                    addChatMessage("✓ Accettato invito! Accesso a partita #" + gameId + "...");
                } else {
                    
                    addChatMessage("✗ Hai rifiutato l'invito. Puoi creare o unirti ad altre partite.");
                }
            });
        });
    }
    
    public void returnToLobby() {
        Platform.runLater(() -> {
            primaryStage.setScene(lobbyScene);
            clearBoard();
            canMove = false;
            isSoloMode = false;
            
            addChatMessage("✓ Tornato alla lobby");
        });
    }
    
    public void addChatMessage(String message) {
        Platform.runLater(() -> {
            String timestamp = java.time.LocalTime.now().format(
                java.time.format.DateTimeFormatter.ofPattern("HH:mm:ss"));
            chatArea.appendText("[" + timestamp + "] " + message + "\n");
        });
    }
    
    public void showAlert(String title, String content, Alert.AlertType type) {
        Platform.runLater(() -> {
            Alert alert = new Alert(type);
            alert.setTitle(title);
            alert.setHeaderText(null);
            alert.setContentText(content);
            alert.showAndWait();
        });
    }
    
    private void clearBoard() {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                cellButtons[i][j].setText("");
                cellButtons[i][j].setDisable(false);
                cellButtons[i][j].setTextFill(Color.web("#ECF0F1"));
            }
        }
    }
    
    public static void main(String[] args) {
        launch(args);
    }
}