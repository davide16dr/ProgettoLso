public class GameController {
    private TrisClientFX client;
    private String[][] board;
    private boolean myTurn;
    private String mySymbol;
    private int currentGameId;
    
    public GameController(TrisClientFX client) {
        this.client = client;
        this.board = new String[3][3];
        resetBoard();
    }
    
    public void resetBoard() {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                board[i][j] = "-";
            }
        }
        myTurn = false;
    }
    
    public void updateBoard(String boardData) {
        String[] cells = boardData.trim().split("\\s+");
        if (cells.length == 9) {
            for (int i = 0; i < 9; i++) {
                int row = i / 3;
                int col = i % 3;
                board[row][col] = cells[i];
            }
        }
    }
    
    public boolean canMakeMove(int row, int col) {
        return myTurn && board[row][col].equals("-");
    }
    
    public void setMyTurn(boolean turn) {
        this.myTurn = turn;
    }
    
    public boolean isMyTurn() {
        return myTurn;
    }
    
    public void setMySymbol(String symbol) {
        this.mySymbol = symbol;
    }
    
    public String getMySymbol() {
        return mySymbol;
    }
    
    public void setCurrentGameId(int gameId) {
        this.currentGameId = gameId;
    }
    
    public int getCurrentGameId() {
        return currentGameId;
    }
    
    public String getCellValue(int row, int col) {
        return board[row][col];
    }
}