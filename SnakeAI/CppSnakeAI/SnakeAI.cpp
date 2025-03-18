#include <iostream>
#include <windows.h>
#include <time.h>
#include <conio.h>
#include <limits>
using namespace std;

#define H 22
#define W 22
#define INF 1000000

struct Pair {
    int x, y;
};

class chessboard {
public:
    char qp[H][W];
    int i, j, x1, y1;
    chessboard();
    void food();
    void prt(int grade, int score, int gamespeed);
};

chessboard::chessboard()
{
    for(i = 1; i <= H - 2; i++)
        for(j = 1; j <= W - 2; j++)
            qp[i][j] = ' ';
    for(i = 0; i <= H - 1; i++)
    {
        qp[0][i] = qp[H-1][i] = '#';
    }
    for(i = 1; i <= H - 2; i++)
    {
        qp[i][0] = qp[i][W-1] = '#';
    }
    food();
}

void chessboard::food()
{
    srand(time(0));
    do {
        x1 = rand() % (W - 2) + 1;
        y1 = rand() % (H - 2) + 1;
    } while(qp[x1][y1] != ' ');
    qp[x1][y1] = '$';
}

void chessboard::prt(int grade, int score, int gamespeed)
{
    system("cls");
    cout << endl;
    for(i = 0; i < H; i++)
    {
        cout << "\t";
        for(j = 0; j < W; j++)
            cout << qp[i][j] << ' ';
        if(i == 0) cout << "\tGrade: " << grade;
        if(i == 2) cout << "\tScore: " << score;
        if(i == 4) cout << "\tAutomatic (A* control)";
        if(i == 5) cout << "\ttime interval: " << gamespeed << "ms";
        cout << endl;
    }
}

class snake : public chessboard {
public:
    int zb[2][100];
    int head, tail, grade, score, gamespeed, length, x, y;
    snake();
    void move();
    bool Astar(int sx, int sy, int foodX, int foodY, int &nextX, int &nextY);
};

snake::snake() : chessboard()
{
    for(i = 1; i <= 3; i++)
        qp[1][i] = '*';
    qp[1][4] = '@';
    for(i = 0; i < 4; i++)
    {
        zb[0][i] = 1;
        zb[1][i] = i + 1;
    }
}

bool snake::Astar(int sx, int sy, int foodX, int foodY, int &nextX, int &nextY)
{
    int g[H][W], f[H][W];
    bool closed[H][W] = {false};
    Pair parent[H][W];
    int dx[4] = {-1, 1, 0, 0};
    int dy[4] = {0, 0, -1, 1};

    for(int i = 0; i < H; i++)
        for(int j = 0; j < W; j++)
        {
            g[i][j] = INF;
            f[i][j] = INF;
            parent[i][j] = {-1, -1};
        }

    g[sx][sy] = 0;
    f[sx][sy] = abs(sx - foodX) + abs(sy - foodY);

    while (true)
    {
        int currentX = -1, currentY = -1;
        int bestF = INF;
        for (int i = 0; i < H; i++)
            for(int j = 0; j < W; j++)
            {
                if(!closed[i][j] && f[i][j] < bestF)
                {
                    bestF = f[i][j];
                    currentX = i;
                    currentY = j;
                }
            }
        if (currentX == -1) break;

        if (currentX == foodX && currentY == foodY)
        {
            int pathX = foodX, pathY = foodY;
            while (!(parent[pathX][pathY].x == sx && parent[pathX][pathY].y == sy))
            {
                int tx = parent[pathX][pathY].x;
                int ty = parent[pathX][pathY].y;
                pathX = tx;
                pathY = ty;
            }
            nextX = foodX == sx ? sx : ((pathX != sx) ? pathX : foodX);
            nextY = foodY == sy ? sy : ((pathY != sy) ? pathY : foodY);
            int cx = foodX, cy = foodY;
            while (!(parent[cx][cy].x == sx && parent[cx][cy].y == sy))
            {
                int px = parent[cx][cy].x;
                int py = parent[cx][cy].y;
                cx = px;
                cy = py;
            }
            nextX = cx;
            nextY = cy;
            return true;
        }

        closed[currentX][currentY] = true;

        for (int d = 0; d < 4; d++)
        {
            int nx = currentX + dx[d];
            int ny = currentY + dy[d];

            if (nx < 0 || nx >= H || ny < 0 || ny >= W) continue;
            if (qp[nx][ny] == '#')
                continue;
            if (qp[nx][ny] == '*' || qp[nx][ny] == '@')
            {
                if (!(nx == foodX && ny == foodY))
                    continue;
            }

            int tentative_g = g[currentX][currentY] + 1;
            if (tentative_g < g[nx][ny])
            {
                parent[nx][ny] = {currentX, currentY};
                g[nx][ny] = tentative_g;
                f[nx][ny] = tentative_g + abs(nx - foodX) + abs(ny - foodY);
            }
        }
    }
    return false;
}

void snake::move()
{
    score = 0;
    head = 3;
    tail = 0;
    grade = 1;
    length = 4;
    gamespeed = 100;

    while (true)
    {
        int sx = zb[0][head], sy = zb[1][head];
        int nx, ny;
        bool pathFound = Astar(sx, sy, x1, y1, nx, ny);
        if (!pathFound)
        {
            cout << "\n\tNo path to food! Game over!" << endl;
            break;
        }
        if (nx == 0 || nx == H-1 || ny == 0 || ny == W-1)
        {
            cout << "\n\tHit the wall! Game over!" << endl;
            break;
        }
        if (!(nx == x1 && ny == y1) && qp[nx][ny] != ' ')
        {
            cout << "\n\tCollision! Game over!" << endl;
            break;
        }
        if (nx == x1 && ny == y1)
        {
            length++;
            score += 100;
            if (length >= 8)
            {
                length -= 8;
                grade++;
                if(gamespeed >= 200)
                    gamespeed = 550 - grade * 50;
            }
            qp[nx][ny] = '@';
            qp[zb[0][head]][zb[1][head]] = '*';
            head = (head+1) % 100;
            zb[0][head] = nx;
            zb[1][head] = ny;
            food();
            prt(grade, score, gamespeed);
        }
        else
        {
            qp[zb[0][tail]][zb[1][tail]] = ' ';
            tail = (tail+1)%100;
            qp[zb[0][head]][zb[1][head]] = '*';
            head = (head+1)%100;
            zb[0][head] = nx;
            zb[1][head] = ny;
            qp[nx][ny] = '@';
            prt(grade, score, gamespeed);
        }
        Sleep(gamespeed);
    }
}

int main()
{
    chessboard cb;
    snake s;
    s.move();
    return 0;
}
