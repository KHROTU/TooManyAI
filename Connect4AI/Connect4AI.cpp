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
const int DRAW_SCORE = 0;
const int MAX_DEPTH = 12;
const int CENTER_WEIGHTS[COLS] = {1, 3, 6, 9, 6, 3, 1};
const int TIMEOUT_MS = 15000;

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
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            for (int k = 0; k < 2; ++k) {
                zobristTable[i][j][k] = gen();
            }
        }
    }
    gameHash = 0;
}

void InitializeWinningLines() {
    winningLines.clear();
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col <= COLS - 4; ++col) {
            winningLines.push_back({{row, col}, {row, col + 1}, {row, col + 2}, {row, col + 3}});
        }
    }
    for (int col = 0; col < COLS; ++col) {
        for (int row = 0; row <= ROWS - 4; ++row) {
            winningLines.push_back({{row, col}, {row + 1, col}, {row + 2, col}, {row + 3, col}});
        }
    }
    for (int row = 0; row <= ROWS - 4; ++row) {
        for (int col = 0; col <= COLS - 4; ++col) {
            winningLines.push_back({{row, col}, {row + 1, col + 1}, {row + 2, col + 2}, {row + 3, col + 3}});
        }
    }
    for (int row = 3; row < ROWS; ++row) {
        for (int col = 0; col <= COLS - 4; ++col) {
            winningLines.push_back({{row, col}, {row - 1, col + 1}, {row - 2, col + 2}, {row - 3, col + 3}});
        }
    }
}

void SetColor(int color) {
    #ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
    #else
    const char* colors[] = {"\033[0;37m", "\033[0;31m", "\033[0;33m"};
    if (color == 7) std::cout << colors[0];
    else if (color == 12) std::cout << colors[1];
    else if (color == 14) std::cout << colors[2];
    else std::cout << colors[0];
    #endif
}

void ClearScreen() {
    #ifdef _WIN32
    system("cls");
    #else
    system("clear");
    #endif
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

bool IsBoardFull() {
    for (int col = 0; col < COLS; ++col) {
        if (board[0][col] == ' ') return false;
    }
    return true;
}

bool CheckWin(char player) {
    for (const auto& line : winningLines) {
        bool win = true;
        for (const auto& pos : line) {
            if (pos.first < 0 || pos.first >= ROWS || pos.second < 0 || pos.second >= COLS || board[pos.first][pos.second] != player) {
                win = false;
                break;
            }
        }
        if (win) return true;
    }
    return false;
}

bool DropPiece(int col, char player) {
    if (col < 0 || col >= COLS || board[0][col] != ' ') return false;
    for (int row = ROWS - 1; row >= 0; --row) {
        if (board[row][col] == ' ') {
            board[row][col] = player;
            int playerIndex = (player == 'Y');
            gameHash ^= zobristTable[row][col][playerIndex];
            return true;
        }
    }
    return false;
}

void UndoPiece(int col) {
    if (col < 0 || col >= COLS) return;
    for (int row = 0; row < ROWS; ++row) {
        if (board[row][col] != ' ') {
            char player = board[row][col];
            int playerIndex = (player == 'Y');
            gameHash ^= zobristTable[row][col][playerIndex];
            board[row][col] = ' ';
            return;
        }
    }
}

int EvaluateBoard() {
    if (CheckWin('Y')) return WIN_SCORE / 2;
    if (CheckWin('R')) return -WIN_SCORE / 2;

    int score = 0;
    int forcedWinScore = 0;
    int opponentBlocks = 0;

    for (const auto& line : winningLines) {
        int playerCount = 0;
        int opponentCount = 0;
        int emptyCount = 0;
        for (const auto& pos : line) {
            if (board[pos.first][pos.second] == 'Y') playerCount++;
            else if (board[pos.first][pos.second] == 'R') opponentCount++;
            else emptyCount++;
        }

        if (playerCount == 3 && emptyCount == 1) score += 100000;
        if (opponentCount == 3 && emptyCount == 1) score -= 500000;
        if (playerCount == 2 && emptyCount == 2) score += 1000;
        if (opponentCount == 2 && emptyCount == 2) score -= 2000;
        if (playerCount == 1 && emptyCount == 3) score += 100;
        if (opponentCount == 1 && emptyCount == 3) score -= 150;
    }

    for (int col = 0; col < COLS; ++col) {
        for (int row = 0; row < ROWS; ++row) {
            if (board[row][col] == 'Y') score += CENTER_WEIGHTS[col] * 10;
            else if (board[row][col] == 'R') score -= CENTER_WEIGHTS[col] * 10;
        }
    }

    for (int col = 0; col < COLS; ++col) {
        if (board[0][col] == ' ') {
            int row = -1;
            for (int r = ROWS - 1; r >= 0; --r) {
                if (board[r][col] == ' ') {
                    row = r;
                    break;
                }
            }
            if (row != -1) {
                board[row][col] = 'Y';
                if (CheckWin('Y')) forcedWinScore += 200000;
                board[row][col] = 'R';
                if (CheckWin('R')) opponentBlocks++;
                board[row][col] = ' ';
            }
        }
    }

    if (forcedWinScore >= 400000) score += forcedWinScore;
    else if (opponentBlocks > 1) score -= 300000;

    return score;
}

int minimax(int depth, int alpha, int beta, char player, int ply) {
    uint64_t currentHash = gameHash;

    if (ply > 2 && std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - searchStart).count() > TIMEOUT_MS) {
        timeout = true;
        return 0;
    }

    if (CheckWin('Y')) return WIN_SCORE - ply;
    if (CheckWin('R')) return -WIN_SCORE + ply;
    if (IsBoardFull()) return DRAW_SCORE;
    if (depth == 0) return EvaluateBoard();

    auto ttEntryIt = transpositionTable.find(currentHash);
    if (ttEntryIt != transpositionTable.end()) {
        TranspositionEntry& entry = ttEntryIt->second;
        if (entry.key == currentHash && entry.depth >= depth) {
            if (entry.flag == 0) return entry.score;
            if (entry.flag == 1 && entry.score >= beta) return entry.score;
            if (entry.flag == 2 && entry.score <= alpha) return entry.score;
            if (entry.flag == 1) alpha = std::max(alpha, entry.score);
            if (entry.flag == 2) beta = std::min(beta, entry.score);
            if (alpha >= beta) return entry.score;
        }
    }

    std::vector<std::pair<int, int>> moves;
    char opponent = (player == 'Y') ? 'R' : 'Y';

    for (int col = 0; col < COLS; ++col) {
        if (board[0][col] == ' ') {
            int priority = 0;
            int row = -1;
            for (int r = ROWS - 1; r >= 0; --r) {
                if (board[r][col] == ' ') {
                    row = r;
                    break;
                }
            }
            if (row == -1) continue;

            board[row][col] = player;
            if (CheckWin(player)) priority = 1000000;
            board[row][col] = ' ';

            if (priority == 0) {
                board[row][col] = opponent;
                if (CheckWin(opponent)) priority = 500000;
                board[row][col] = ' ';
            }

            if (priority == 0) {
                board[row][col] = player;
                priority = EvaluateBoard();
                board[row][col] = ' ';
                priority = (player == 'Y') ? priority : -priority;
            }

            moves.emplace_back(-priority, col);
        }
    }
    std::sort(moves.begin(), moves.end());

    int bestScore = (player == 'Y') ? -std::numeric_limits<int>::max() : std::numeric_limits<int>::max();
    int alphaOrig = alpha;

    for (const auto& move : moves) {
        int col = move.second;
        DropPiece(col, player);
        int score;
        if (player == 'Y') {
            score = minimax(depth - 1, alpha, beta, 'R', ply + 1);
            bestScore = std::max(bestScore, score);
            alpha = std::max(alpha, bestScore);
        } else {
            score = minimax(depth - 1, alpha, beta, 'Y', ply + 1);
            bestScore = std::min(bestScore, score);
            beta = std::min(beta, bestScore);
        }
        UndoPiece(col);
        if (timeout) return bestScore;
        if (alpha >= beta) break;
    }

    TranspositionEntry newEntry;
    newEntry.depth = depth;
    newEntry.score = bestScore;
    newEntry.key = currentHash;
    if (player == 'Y') {
        if (bestScore <= alphaOrig) newEntry.flag = 2;
        else if (bestScore >= beta) newEntry.flag = 1;
        else newEntry.flag = 0;
    } else {
        if (bestScore <= alphaOrig) newEntry.flag = 1;
        else if (bestScore >= beta) newEntry.flag = 2;
        else newEntry.flag = 0;
    }
    if (!timeout) transpositionTable[currentHash] = newEntry;

    return bestScore;
}

int FindBestMove() {
    if (transpositionTable.size() > 1000000) {
        transpositionTable.clear();
    }
    timeout = false;
    searchStart = std::chrono::steady_clock::now();
    int bestCol = -1;
    int bestScoreOverall = -std::numeric_limits<int>::max();
    int lastCompletedDepthBestCol = board[0][COLS/2] == ' ' ? COLS/2 : 0;

    std::cout << "AI thinking..." << std::endl;

    for (int depth = 1; depth <= MAX_DEPTH; ++depth) {
        int currentDepthBestCol = -1;
        int currentDepthBestScore = -std::numeric_limits<int>::max();
        std::vector<std::pair<int, int>> moves;

        for (int col = 0; col < COLS; ++col) {
            if (board[0][col] == ' ') {
                int priority = (col == bestCol) ? 1000 : CENTER_WEIGHTS[col];
                int row = -1;
                for (int r = ROWS - 1; r >= 0; --r) {
                    if (board[r][col] == ' ') {
                        row = r;
                        break;
                    }
                }
                if (row != -1) {
                    board[row][col] = 'Y';
                    if (CheckWin('Y')) priority = 2000000;
                    board[row][col] = ' ';
                    board[row][col] = 'R';
                    if (CheckWin('R')) priority = 1000000;
                    board[row][col] = ' ';
                }
                moves.emplace_back(-priority, col);
            }
        }
        std::sort(moves.begin(), moves.end());

        std::cout << "Depth " << depth << ": ";
        auto depthStart = std::chrono::steady_clock::now();

        for (const auto& [_, col] : moves) {
            DropPiece(col, 'Y');
            int score = minimax(depth - 1, -std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), 'R', 1);
            UndoPiece(col);
            if (timeout) {
                std::cout << " Timeout!" << std::endl;
                break;
            }
            if (score > currentDepthBestScore) {
                currentDepthBestScore = score;
                currentDepthBestCol = col;
                std::cout << " col " << (col + 1) << " (score " << score << ")";
                std::cout.flush();
            }
            if (currentDepthBestScore >= WIN_SCORE - MAX_DEPTH) break;
        }

        auto depthEnd = std::chrono::steady_clock::now();
        long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(depthEnd - depthStart).count();
        std::cout << " [" << duration << "ms]" << std::endl;

        if (!timeout) {
            bestCol = currentDepthBestCol;
            bestScoreOverall = currentDepthBestScore;
            lastCompletedDepthBestCol = bestCol;
            std::cout << "  -> Best move after depth " << depth << ": Column " << (bestCol + 1) << " (Score: " << bestScoreOverall << ")" << std::endl;
            if (bestScoreOverall >= WIN_SCORE - MAX_DEPTH) {
                std::cout << "  -> Found winning sequence!" << std::endl;
                break;
            }
        } else {
            std::cout << "  -> Timeout during depth " << depth << ". Using best move from depth " << (depth - 1) << "." << std::endl;
            break;
        }

        auto timeSoFar = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - searchStart).count();
        if (timeSoFar * 3 > TIMEOUT_MS && depth < MAX_DEPTH) {
            std::cout << "  -> Halting early, likely insufficient time for depth " << (depth + 1) << std::endl;
            break;
        }
    }

    std::cout << "AI chooses column: " << (lastCompletedDepthBestCol + 1) << std::endl;
    return lastCompletedDepthBestCol;
}

int main() {
    InitializeZobrist();
    InitializeWinningLines();

    std::cout << "Welcome to Connect 4 vs AI!\n";
    std::cout << "Choose who starts:\n1. You (Red - R)\n2. AI (Yellow - Y)\nEnter 1 or 2: ";
    int choice;
    while (!(std::cin >> choice) || (choice != 1 && choice != 2)) {
        std::cout << "Invalid input. Please enter 1 or 2: ";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    currentPlayer = (choice == 1) ? 'R' : 'Y';

    bool gameOver = false;
    while (!gameOver) {
        ClearScreen();
        DisplayBoard();

        char winner = ' ';

        if (currentPlayer == 'R') {
            int moveCol = -1;
            bool validMove = false;
            while (!validMove) {
                std::cout << "Your move (1-" << COLS << "): ";
                int input;
                if (!(std::cin >> input) || input < 1 || input > COLS) {
                    std::cout << "Invalid input. Please enter a number between 1 and " << COLS << ".\n";
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    continue;
                }
                moveCol = input - 1;
                if (board[0][moveCol] != ' ') {
                    std::cout << "Column " << input << " is full. Try again.\n";
                    continue;
                }
                validMove = DropPiece(moveCol, 'R');
                if (!validMove) {
                    std::cout << "Error dropping piece. Try again.\n";
                }
            }
            if (CheckWin('R')) {
                winner = 'R';
                gameOver = true;
            }
        } else {
            std::cout << "AI's turn ('Y')..." << std::endl;
            int aiMoveCol = FindBestMove();
            if (aiMoveCol != -1) {
                DropPiece(aiMoveCol, 'Y');
                if (CheckWin('Y')) {
                    winner = 'Y';
                    gameOver = true;
                }
            } else {
                std::cout << "AI Error: No valid move found?" << std::endl;
                for (int c = 0; c < COLS; ++c) {
                    if (board[0][c] == ' ') {
                        DropPiece(c, 'Y');
                        if (CheckWin('Y')) winner = 'Y';
                        gameOver = true;
                        break;
                    }
                }
            }
        }

        if (!gameOver && IsBoardFull()) {
            winner = 'D';
            gameOver = true;
        }

        if (!gameOver) {
            currentPlayer = (currentPlayer == 'R') ? 'Y' : 'R';
        } else {
            ClearScreen();
            DisplayBoard();
            if (winner == 'R') std::cout << "Congratulations! You (Red) win!\n";
            else if (winner == 'Y') std::cout << "AI (Yellow) wins!\n";
            else if (winner == 'D') std::cout << "It's a draw!\n";
            else std::cout << "Game Over!\n";
        }
    }

    #ifdef _WIN32
    system("pause");
    #else
    std::cout << "Press Enter to exit...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
    #endif
    return 0;
}