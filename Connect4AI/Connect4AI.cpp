#define NOMINMAX
#include <iostream>
#include <vector>
#include <unordered_map>
#include <windows.h>
#include <limits>
#include <algorithm>
#include <chrono>
#include <random>
#include <bitset>

const int ROWS = 6;
const int COLS = 7;
const int WIN_SCORE = 100000000;
const int MAX_DEPTH = 16;
const int CENTER_WEIGHTS[COLS] = {1, 3, 6, 9, 6, 3, 1};
const int TIMEOUT_MS = 10000;

struct TranspositionEntry {
    int depth;
    int score;
    int flag;
    uint64_t key;
};

std::vector<std::vector<char>> board(ROWS, std::vector<char>(COLS, ' '));
std::unordered_map<uint64_t, TranspositionEntry> transpositionTable;
char currentPlayer = 'R';
std::vector<std::vector<std::pair<int, int>>> winningLines;
std::chrono::time_point<std::chrono::steady_clock> searchStart;
bool timeout = false;

uint64_t zobristTable[ROWS][COLS][2];
uint64_t gameHash = 0;

void InitializeZobrist() {
    std::mt19937_64 gen(0xDEADBEEF);
    for (int i = 0; i < ROWS; ++i)
        for (int j = 0; j < COLS; ++j)
            for (int k = 0; k < 2; ++k)
                zobristTable[i][j][k] = gen();
}

void InitializeWinningLines() {
    winningLines.clear();
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col <= COLS-4; ++col)
            winningLines.push_back({{row,col}, {row,col+1}, {row,col+2}, {row,col+3}});
    
    for (int col = 0; col < COLS; ++col)
        for (int row = 0; row <= ROWS-4; ++row)
            winningLines.push_back({{row,col}, {row+1,col}, {row+2,col}, {row+3,col}});
    
    for (int row = 0; row <= ROWS-4; ++row)
        for (int col = 0; col <= COLS-4; ++col)
            winningLines.push_back({{row,col}, {row+1,col+1}, {row+2,col+2}, {row+3,col+3}});
    
    for (int row = 3; row < ROWS; ++row)
        for (int col = 0; col <= COLS-4; ++col)
            winningLines.push_back({{row,col}, {row-1,col+1}, {row-2,col+2}, {row-3,col+3}});
}

void SetColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void ClearScreen() {
    system("cls");
}

void DisplayBoard() {
    std::cout << "\n";
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            std::cout << "| ";
            if (board[row][col] == 'R') SetColor(12);
            else if (board[row][col] == 'Y') SetColor(14);
            else SetColor(7);
            std::cout << board[row][col];
            SetColor(7);
            std::cout << " ";
        }
        std::cout << "|\n";
    }
    std::cout << "+---+---+---+---+---+---+---+\n";
    std::cout << "  1   2   3   4   5   6   7\n";
}

bool CheckWin(char player) {
    for (const auto& line : winningLines) {
        bool win = true;
        for (const auto& pos : line)
            if (board[pos.first][pos.second] != player) {
                win = false;
                break;
            }
        if (win) return true;
    }
    return false;
}

bool DropPiece(int col, char player) {
    for (int row = ROWS-1; row >= 0; --row) {
        if (board[row][col] == ' ') {
            board[row][col] = player;
            gameHash ^= zobristTable[row][col][player == 'Y'];
            return true;
        }
    }
    return false;
}

int EvaluateThreats(char player, bool currentTurn) {
    char opponent = (player == 'R') ? 'Y' : 'R';
    int score = 0;
    
    for (int col = 0; col < COLS; ++col) {
        if (board[0][col] != ' ') continue;
        int row = ROWS-1;
        while (row >= 0 && board[row][col] != ' ') row--;
        
        board[row][col] = player;
        if (CheckWin(player)) score += WIN_SCORE;
        board[row][col] = ' ';
        
        board[row][col] = opponent;
        if (CheckWin(opponent)) score -= WIN_SCORE;
        board[row][col] = ' ';
    }

    for (const auto& line : winningLines) {
        int playerCount = 0, opponentCount = 0, empty = 0;
        for (const auto& pos : line) {
            if (board[pos.first][pos.second] == player) playerCount++;
            else if (board[pos.first][pos.second] == opponent) opponentCount++;
            else empty++;
        }
        
        if (playerCount == 3 && empty == 1) score += currentTurn ? 250000 : 125000;
        if (opponentCount == 3 && empty == 1) score -= currentTurn ? 300000 : 150000;
        if (playerCount == 2 && empty == 2) score += currentTurn ? 5000 : 2500;
        if (opponentCount == 2 && empty == 2) score -= currentTurn ? 6000 : 3000;
    }
    
    for (int col = 0; col < COLS; ++col)
        score += CENTER_WEIGHTS[col] * (board[ROWS/2][col] == player ? 10 : 0);

    return score;
}

int PVS(int depth, int alpha, int beta, bool maximizing, int ply) {
    if (CheckWin('Y')) return WIN_SCORE - ply;
    if (CheckWin('R')) return -WIN_SCORE + ply;
    if (depth == 0) return EvaluateThreats(maximizing ? 'Y' : 'R', maximizing);

    auto ttEntry = transpositionTable.find(gameHash);
    if (ttEntry != transpositionTable.end() && ttEntry->second.depth >= depth) {
        if (ttEntry->second.flag == 0) return ttEntry->second.score;
        if (ttEntry->second.flag == 1 && ttEntry->second.score >= beta) return beta;
        if (ttEntry->second.flag == 2 && ttEntry->second.score <= alpha) return alpha;
    }

    std::vector<std::pair<int, int>> moves;
    char currentPlayerSymbol = maximizing ? 'Y' : 'R';
    for (int col = 0; col < COLS; ++col) {
        if (board[0][col] != ' ') continue;
        int priority = CENTER_WEIGHTS[col] * 10;
        
        int row = ROWS-1;
        while (row >= 0 && board[row][col] != ' ') row--;
        board[row][col] = currentPlayerSymbol;
        if (CheckWin(currentPlayerSymbol)) priority += 10000000;
        board[row][col] = ' ';
        
        moves.emplace_back(-priority, col);
    }
    std::sort(moves.begin(), moves.end());

    int bestScore = -std::numeric_limits<int>::max();
    int bestMove = moves.front().second;
    int alphaOrig = alpha;

    for (size_t i = 0; i < moves.size(); ++i) {
        int col = moves[i].second;
        int row = ROWS-1;
        while (row >= 0 && board[row][col] != ' ') row--;
        
        board[row][col] = maximizing ? 'Y' : 'R';
        gameHash ^= zobristTable[row][col][maximizing ? 1 : 0];
        
        int score;
        if (i == 0) {
            score = -PVS(depth-1, -beta, -alpha, !maximizing, ply+1);
        } else {
            score = -PVS(depth-1, -alpha-1, -alpha, !maximizing, ply+1);
            if (score > alpha && score < beta)
                score = -PVS(depth-1, -beta, -score, !maximizing, ply+1);
        }
        
        board[row][col] = ' ';
        gameHash ^= zobristTable[row][col][maximizing ? 1 : 0];
        
        if (score > bestScore) {
            bestScore = score;
            bestMove = col;
        }
        alpha = std::max(alpha, bestScore);
        if (alpha >= beta) break;
        
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - searchStart).count() > TIMEOUT_MS) {
            timeout = true;
            break;
        }
    }

    TranspositionEntry entry;
    entry.depth = depth;
    entry.score = bestScore;
    entry.flag = (bestScore <= alphaOrig) ? 2 : (bestScore >= beta) ? 1 : 0;
    entry.key = gameHash;
    transpositionTable[gameHash] = entry;

    return bestScore;
}

int FindBestMove() {
    transpositionTable.clear();
    timeout = false;
    searchStart = std::chrono::steady_clock::now();
    int bestCol = 3, bestScore = -std::numeric_limits<int>::max();

    for (int depth = 1; depth <= MAX_DEPTH && !timeout; ++depth) {
        int currentBest = bestCol;
        int currentScore = -std::numeric_limits<int>::max();
        
        std::vector<std::pair<int, int>> moves;
        for (int col = 0; col < COLS; ++col) {
            if (board[0][col] == ' ') {
                int priority = CENTER_WEIGHTS[col] * 10;
                if (col == bestCol) priority += 5000;
                moves.emplace_back(-priority, col);
            }
        }
        std::sort(moves.begin(), moves.end());

        for (const auto& [priority, col] : moves) {
            int row = ROWS-1;
            while (row >= 0 && board[row][col] != ' ') row--;
            
            board[row][col] = 'Y';
            gameHash ^= zobristTable[row][col][1];
            int score = -PVS(depth-1, -std::numeric_limits<int>::max(), 
                           std::numeric_limits<int>::max(), false, 0);
            board[row][col] = ' ';
            gameHash ^= zobristTable[row][col][1];
            
            if (score > currentScore) {
                currentScore = score;
                currentBest = col;
            }
            if (timeout) break;
        }
        
        if (!timeout) {
            bestCol = currentBest;
            bestScore = currentScore;
        }
    }
    return bestCol;
}

int main() {
    InitializeZobrist();
    InitializeWinningLines();
    std::cout << "Welcome to Connect 4 vs AI!\n";
    std::cout << "Choose who starts:\n1. You (Red - R)\n2. AI (Yellow - Y)\nEnter 1 or 2: ";
    int choice;
    std::cin >> choice;
    currentPlayer = (choice == 1) ? 'R' : 'Y';

    while (true) {
        ClearScreen();
        DisplayBoard();
        
        if (currentPlayer == 'R') {
            int move;
            std::cout << "Your move (1-7): ";
            std::cin >> move;
            if (move < 1 || move > 7) continue;
            if (!DropPiece(move-1, 'R')) continue;
        } else {
            int aiMove = FindBestMove();
            DropPiece(aiMove, 'Y');
        }
        
        if (CheckWin(currentPlayer)) {
            ClearScreen();
            DisplayBoard();
            std::cout << (currentPlayer == 'R' ? "You win!" : "AI wins!") << "\n";
            break;
        }
        currentPlayer = (currentPlayer == 'R') ? 'Y' : 'R';
    }
    system("pause");
    return 0;
}