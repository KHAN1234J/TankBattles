#include <cmath>
#include <conio.h>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
using namespace std;

const int MAP_WIDTH = 60;
const int MAP_HEIGHT = 20;
const int MAX_BULLETS = 20;
const int MAX_PARTICLES = 30;

const int DIR_UP = 0;
const int DIR_DOWN = 1;
const int DIR_LEFT = 2;
const int DIR_RIGHT = 3;
const string C_RESET = "\033[0m";
const string C_GREEN = "\033[92m";
const string C_CYAN = "\033[96m";
const string C_RED = "\033[91m";
const string C_YELLOW = "\033[93m";
const string C_GRAY = "\033[90m";
const string C_WHITE = "\033[97m";

// --- UTF-8 Symbols ---
const string WALL_BLOCK = "\xE2\x96\x88";
const string CRATE_BLOCK = "\xE2\x96\x92";
const string HEART = "\xE2\x99\xA5";

// --- Structures ---
struct Tank {
  int x, y, dir, hp;
  string color;
  bool active;
};

struct Bullet {
  int x, y, dir;
  string color;
  bool active;
};

struct Particle {
  int x, y, life;
  string character;
  string color;
  bool active;
};

// --- Globals ---
string screenBuffer[MAP_HEIGHT][MAP_WIDTH];
char mapGrid[MAP_HEIGHT][MAP_WIDTH];
Tank p1, p2;
Bullet bullets[MAX_BULLETS];
Particle particles[MAX_PARTICLES];

bool appRunning = true;
bool gameRunning = false;
int winner = 0;
bool matchEnded = false;
int postGameDelay = 0;

// --- Function Prototypes ---
void setupConsole();
void showMainMenu();
void viewMatchHistory();
void initGame();
void drawGame();
void updateGame();
void handleInput();
void saveScore();
void clearBuffer();
void drawToBuffer(int x, int y, string text, string color);
void drawTankSprite(int x, int y, int dir, string color);
bool isValidPosition(int cx, int cy);
void shoot(int cx, int cy, int dir, string color);
void spawnParticle(int x, int y, string character, string color);
void spawnTankExplosion(int tx, int ty);
void drawTextToBuffer(int x, int y, string text, string color);

// --- Main Function ---
int main() {
  setupConsole();
  srand(time(0));

  while (appRunning) {
    showMainMenu();
    char choice = _getch();

    if (choice == '1') {
      initGame();
      gameRunning = true;
      while (gameRunning) {
        handleInput();
        updateGame();
        drawGame();
        Sleep(50); // 20 FPS Control
      }

      saveScore();
    } else if (choice == '2') {
      viewMatchHistory();
    } else if (choice == '3') {
      appRunning = false;
    }
  }

  return 0;
}

// --- Console Setup (Enables ANSI and UTF-8) ---
void setupConsole() {
  SetConsoleOutputCP(CP_UTF8);
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(hOut, dwMode);

  // Hide the cursor
  CONSOLE_CURSOR_INFO cursorInfo;
  GetConsoleCursorInfo(hOut, &cursorInfo);
  cursorInfo.bVisible = FALSE;
  SetConsoleCursorInfo(hOut, &cursorInfo);
}

// --- Main Menu ---
void showMainMenu() {
  system("cls");
  cout << C_GREEN << R"(
    ========================================================
      _______       _   _  _  __   ____        _   _   _
     |__   __|     | \ | || |/ /  |  _ \      | | | | | |
        | |  __ _  |  \| || ' /   | |_) | __ _| |_| |_| | ___
        | | / _` | | . ` ||  <    |  _ < / _` | __| __| |/ _ \
        | || (_| | | |\  || . \   | |_) | (_| | |_| |_| |  __/
        |_| \__,_| |_| \_||_|\_\  |____/ \__,_|\__|\__|_|\___|
    ========================================================
    )" << C_RESET
       << "\n";
  cout << C_CYAN << "            [1] Start Local Tank Duel\n";
  cout << "            [2] View Match History\n";
  cout << "            [3] Quit Game\n" << C_RESET;
  cout << "\n    Select an option: ";
}

void viewMatchHistory() {
  system("cls");
  cout << C_YELLOW << "=== MATCH HISTORY ===\n\n" << C_RESET;
  ifstream file("scores.txt");
  if (file.is_open()) {
    string line;
    while (getline(file, line)) {
      cout << "  " << line << "\n";
    }
    file.close();
  } else {
    cout << "  No match history found yet.\n";
  }
  cout << C_GRAY << "\nPress any key to return..." << C_RESET;
  _getch();
}

// --- Initialization ---
void initGame() {
  // Generate borders and empty space
  for (int i = 0; i < MAP_HEIGHT; i++) {
    for (int j = 0; j < MAP_WIDTH; j++) {
      if (i == 0 || i == MAP_HEIGHT - 1 || j == 0 || j == MAP_WIDTH - 1)
        mapGrid[i][j] = '#';
      else
        mapGrid[i][j] = ' ';
    }
  }

  // Add destructible crates
  for (int k = 0; k < 30; k++) {
    int rx = 5 + rand() % (MAP_WIDTH - 10);
    int ry = 3 + rand() % (MAP_HEIGHT - 6);
    mapGrid[ry][rx] = 'X';
  }

  // Center Wall
  for (int i = 6; i < 14; i++)
    mapGrid[i][MAP_WIDTH / 2] = '#';

  // Player 1
  p1.x = 4;
  p1.y = MAP_HEIGHT / 2;
  p1.dir = DIR_RIGHT;
  p1.hp = 5;
  p1.color = C_GREEN;
  p1.active = true;

  // Player 2
  p2.x = MAP_WIDTH - 5;
  p2.y = MAP_HEIGHT / 2;
  p2.dir = DIR_LEFT;
  p2.hp = 5;
  p2.color = C_CYAN;
  p2.active = true;

  for (int i = 0; i < MAX_BULLETS; i++)
    bullets[i].active = false;
  for (int i = 0; i < MAX_PARTICLES; i++)
    particles[i].active = false;

  matchEnded = false;
  postGameDelay = 0;
  winner = 0;
}

// --- Buffer Drawing System (Zero Flicker!) ---
void clearBuffer() {
  for (int i = 0; i < MAP_HEIGHT; i++) {
    for (int j = 0; j < MAP_WIDTH; j++) {
      screenBuffer[i][j] = " ";
    }
  }
}

void drawToBuffer(int x, int y, string text, string color) {
  if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
    screenBuffer[y][x] = color + text + C_RESET;
  }
}

void drawTankSprite(int x, int y, int dir, string color) {
  const string up[3][3] = {{" ", "\xE2\x95\x91", " "},
                           {WALL_BLOCK, "\xE2\x96\x80", WALL_BLOCK},
                           {WALL_BLOCK, " ", WALL_BLOCK}};
  const string down[3][3] = {{WALL_BLOCK, " ", WALL_BLOCK},
                             {WALL_BLOCK, "\xE2\x96\x84", WALL_BLOCK},
                             {" ", "\xE2\x95\x91", " "}};
  const string left[3][3] = {{WALL_BLOCK, WALL_BLOCK, " "},
                             {"\xE2\x95\x90", WALL_BLOCK, WALL_BLOCK},
                             {WALL_BLOCK, WALL_BLOCK, " "}};
  const string right[3][3] = {{" ", WALL_BLOCK, WALL_BLOCK},
                              {WALL_BLOCK, WALL_BLOCK, "\xE2\x95\x90"},
                              {" ", WALL_BLOCK, WALL_BLOCK}};

  const string(*sprite)[3];
  if (dir == DIR_UP)
    sprite = up;
  else if (dir == DIR_DOWN)
    sprite = down;
  else if (dir == DIR_LEFT)
    sprite = left;
  else
    sprite = right;

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (sprite[i][j] != " ") {
        drawToBuffer(x - 1 + j, y - 1 + i, sprite[i][j], color);
      }
    }
  }
}

void drawGame() {
  clearBuffer();

  // 1. Draw Map to Buffer
  for (int i = 0; i < MAP_HEIGHT; i++) {
    for (int j = 0; j < MAP_WIDTH; j++) {
      if (mapGrid[i][j] == '#')
        drawToBuffer(j, i, WALL_BLOCK, C_GRAY);
      else if (mapGrid[i][j] == 'X')
        drawToBuffer(j, i, CRATE_BLOCK, C_YELLOW);
    }
  }

  // 2. Draw Particles
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].active) {
      drawToBuffer(particles[i].x, particles[i].y, particles[i].character,
                   particles[i].color);
    }
  }

  // 3. Draw Bullets
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      drawToBuffer(bullets[i].x, bullets[i].y, "o", bullets[i].color);
    }
  }

  // 4. Draw Tanks
  if (p1.active)
    drawTankSprite(p1.x, p1.y, p1.dir, p1.color);
  if (p2.active)
    drawTankSprite(p2.x, p2.y, p2.dir, p2.color);

  // 5. Draw Winner Overlay
  if (matchEnded) {
    int boxW = 42;
    int boxH = 8;
    int startX = (MAP_WIDTH - boxW) / 2;
    int startY = (MAP_HEIGHT - boxH) / 2 - 1;

    // Clear background for the box
    for (int y = startY; y < startY + boxH; y++) {
      for (int x = startX; x < startX + boxW; x++) {
        drawToBuffer(x, y, " ", C_RESET);
      }
    }

    // Draw borders
    for (int x = startX + 1; x < startX + boxW - 1; x++) {
      drawToBuffer(x, startY, "\xE2\x95\x90", C_GRAY);
      drawToBuffer(x, startY + boxH - 1, "\xE2\x95\x90", C_GRAY);
    }
    for (int y = startY + 1; y < startY + boxH - 1; y++) {
      drawToBuffer(startX, y, "\xE2\x95\x91", C_GRAY);
      drawToBuffer(startX + boxW - 1, y, "\xE2\x95\x91", C_GRAY);
    }
    drawToBuffer(startX, startY, "\xE2\x95\x94", C_GRAY);
    drawToBuffer(startX + boxW - 1, startY, "\xE2\x95\x97", C_GRAY);
    drawToBuffer(startX, startY + boxH - 1, "\xE2\x95\x9A", C_GRAY);
    drawToBuffer(startX + boxW - 1, startY + boxH - 1, "\xE2\x95\x9D", C_GRAY);

    // Draw Title
    string title = "* MATCH OVER *";
    drawTextToBuffer(startX + (boxW - title.length()) / 2, startY + 1, title, C_YELLOW);

    // Draw Winner Message
    string winMsg = "";
    string winColor = C_RESET;
    if (winner == 1) {
      winMsg = "PLAYER 1 (GREEN) WINS!";
      winColor = C_GREEN;
    } else if (winner == 2) {
      winMsg = "PLAYER 2 (CYAN) WINS!";
      winColor = C_CYAN;
    } else {
      winMsg = "IT'S A DRAW!";
      winColor = C_YELLOW;
    }
    drawTextToBuffer(startX + (boxW - winMsg.length()) / 2, startY + 3, winMsg, winColor);

    // Draw Return Prompt
    if (postGameDelay <= 0) {
      string prompt = "Press ENTER to return to menu";
      drawTextToBuffer(startX + (boxW - prompt.length()) / 2, startY + 5, prompt, C_GRAY);
    }
  }

  // Compile entire buffer into a single string
  string output = "";
  for (int i = 0; i < MAP_HEIGHT; i++) {
    for (int j = 0; j < MAP_WIDTH; j++) {
      output += screenBuffer[i][j];
    }
    output += "\n";
  }

  // Append Aesthetic Dashboard UI
  output += C_GRAY +
            "╔══════════════════════════════════════════════════════════╗\n" +
            C_RESET;
  output += C_GRAY + "║ " + C_GREEN + "PLAYER 1 [W/A/S/D] + [SPACE]   HP: ";
  for (int i = 0; i < 5; i++)
    output += (i < p1.hp ? C_RED + HEART : " ") + " ";
  output += string(9, ' ') + C_GRAY + "║\n";

  output += C_GRAY + "║ " + C_CYAN + "PLAYER 2 [ARROWS]  + [ENTER]   HP: ";
  for (int i = 0; i < 5; i++)
    output += (i < p2.hp ? C_RED + HEART : " ") + " ";
  output += string(9, ' ') + C_GRAY + "║\n";
  output += C_GRAY +
            "╚══════════════════════════════════════════════════════════╝\n" +
            C_RESET;

  // Output frame instantly to prevent flicker
  COORD coord = {0, 0};
  SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
  cout << output;
}

// --- Logic ---
bool isValidPosition(int cx, int cy) {
  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      if (cy + i < 0 || cy + i >= MAP_HEIGHT || cx + j < 0 ||
          cx + j >= MAP_WIDTH)
        return false;
      if (mapGrid[cy + i][cx + j] != ' ')
        return false;
    }
  }
  return true;
}

void shoot(int cx, int cy, int dir, string color) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].active = true;
      bullets[i].dir = dir;
      bullets[i].color = color;
      if (dir == DIR_UP) {
        bullets[i].x = cx;
        bullets[i].y = cy - 2;
      } else if (dir == DIR_DOWN) {
        bullets[i].x = cx;
        bullets[i].y = cy + 2;
      } else if (dir == DIR_LEFT) {
        bullets[i].x = cx - 2;
        bullets[i].y = cy;
      } else if (dir == DIR_RIGHT) {
        bullets[i].x = cx + 2;
        bullets[i].y = cy;
      }
      Beep(800, 40);
      break;
    }
  }
}

void spawnParticle(int x, int y, string character, string color) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) {
      particles[i].active = true;
      particles[i].x = x;
      particles[i].y = y;
      particles[i].character = character;
      particles[i].color = color;
      particles[i].life = 3;
      break;
    }
  }
}

void spawnTankExplosion(int tx, int ty) {
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      spawnParticle(tx + dx, ty + dy, "*", C_RED);
      spawnParticle(tx + dx, ty + dy, "#", C_YELLOW);
    }
  }
  Beep(150, 300);
}

void drawTextToBuffer(int x, int y, string text, string color) {
  for (size_t i = 0; i < text.length(); i++) {
    string temp(1, text[i]);
    drawToBuffer(x + i, y, temp, color);
  }
}

void updateGame() {
  // Update Particles
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].active) {
      particles[i].life--;
      if (particles[i].life <= 0)
        particles[i].active = false;
    }
  }

  // Update Bullets
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      int nx = bullets[i].x;
      int ny = bullets[i].y;

      if (bullets[i].dir == DIR_UP)
        ny--;
      else if (bullets[i].dir == DIR_DOWN)
        ny++;
      else if (bullets[i].dir == DIR_LEFT)
        nx--;
      else if (bullets[i].dir == DIR_RIGHT)
        nx++;

      // Map Bounds & Walls
      if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT ||
          mapGrid[ny][nx] == '#') {
        bullets[i].active = false;
        spawnParticle(nx, ny, "*", C_GRAY);
      }
      // Crates
      else if (mapGrid[ny][nx] == 'X') {
        mapGrid[ny][nx] = ' ';
        bullets[i].active = false;
        spawnParticle(nx, ny, "#", C_YELLOW);
        spawnParticle(nx + 1, ny, ".", C_YELLOW);
        Beep(200, 50);
      }
      // P1 Hit
      else if (p1.active && abs(nx - p1.x) <= 1 && abs(ny - p1.y) <= 1) {
        p1.hp--;
        bullets[i].active = false;
        spawnParticle(nx, ny, "X", C_RED);
        Beep(400, 150);
      }
      // P2 Hit
      else if (p2.active && abs(nx - p2.x) <= 1 && abs(ny - p2.y) <= 1) {
        p2.hp--;
        bullets[i].active = false;
        spawnParticle(nx, ny, "X", C_RED);
        Beep(400, 150);
      } else {
        bullets[i].x = nx;
        bullets[i].y = ny;
      }
    }
  }

  if (matchEnded) {
    if (postGameDelay > 0) {
      postGameDelay--;
    }
    return;
  }

  // Win condition check
  if (p1.hp <= 0 && p2.hp <= 0) {
    winner = 0;
    p1.active = false;
    p2.active = false;
    matchEnded = true;
    postGameDelay = 30;
    spawnTankExplosion(p1.x, p1.y);
    spawnTankExplosion(p2.x, p2.y);
  } else if (p1.hp <= 0) {
    winner = 2;
    p1.active = false;
    matchEnded = true;
    postGameDelay = 30;
    spawnTankExplosion(p1.x, p1.y);
  } else if (p2.hp <= 0) {
    winner = 1;
    p2.active = false;
    matchEnded = true;
    postGameDelay = 30;
    spawnTankExplosion(p2.x, p2.y);
  }
}

void handleInput() {
  static int p1Cooldown = 0;
  static int p2Cooldown = 0;
  static int moveTick = 0;

  moveTick++;
  bool canMove = (moveTick % 2 == 0);

  if (p1Cooldown > 0)
    p1Cooldown--;
  if (p2Cooldown > 0)
    p2Cooldown--;

  if (matchEnded) {
    if (postGameDelay <= 0) {
      if ((GetAsyncKeyState(VK_RETURN) & 0x8000) || (GetAsyncKeyState(VK_ESCAPE) & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
        gameRunning = false;
      }
    }
    return;
  }

  // P1 Keyboard check
  if (p1.active) {
    int nx = p1.x, ny = p1.y;
    bool moved = false;

    if (GetAsyncKeyState('W') & 0x8000) {
      p1.dir = DIR_UP;
      ny--;
      moved = true;
    } else if (GetAsyncKeyState('S') & 0x8000) {
      p1.dir = DIR_DOWN;
      ny++;
      moved = true;
    } else if (GetAsyncKeyState('A') & 0x8000) {
      p1.dir = DIR_LEFT;
      nx--;
      moved = true;
    } else if (GetAsyncKeyState('D') & 0x8000) {
      p1.dir = DIR_RIGHT;
      nx++;
      moved = true;
    }

    bool collisionP2 =
        p2.active && (abs(nx - p2.x) <= 2) && (abs(ny - p2.y) <= 2);

    if (canMove && moved && isValidPosition(nx, ny) && !collisionP2) {
      p1.x = nx;
      p1.y = ny;
    }

    if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && p1Cooldown == 0) {
      shoot(p1.x, p1.y, p1.dir, p1.color);
      p1Cooldown = 8;
    }
  }

  // P2 Keyboard check
  if (p2.active) {
    int nx = p2.x, ny = p2.y;
    bool moved = false;

    if (GetAsyncKeyState(VK_UP) & 0x8000) {
      p2.dir = DIR_UP;
      ny--;
      moved = true;
    } else if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
      p2.dir = DIR_DOWN;
      ny++;
      moved = true;
    } else if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
      p2.dir = DIR_LEFT;
      nx--;
      moved = true;
    } else if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
      p2.dir = DIR_RIGHT;
      nx++;
      moved = true;
    }

    bool collisionP1 =
        p1.active && (abs(nx - p1.x) <= 2) && (abs(ny - p1.y) <= 2);

    if (canMove && moved && isValidPosition(nx, ny) && !collisionP1) {
      p2.x = nx;
      p2.y = ny;
    }

    if ((GetAsyncKeyState(VK_RETURN) & 0x8000) && p2Cooldown == 0) {
      shoot(p2.x, p2.y, p2.dir, p2.color);
      p2Cooldown = 8;
    }
  }

  if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
    gameRunning = false;
  }
}

// --- File I/O ---
void saveScore() {
  ofstream file("scores.txt", ios::app);
  if (file.is_open()) {
    time_t now = time(0);
#pragma warning(suppress : 4996)
    char *dt = ctime(&now);
    if (dt != NULL) {
      if (dt[strlen(dt) - 1] == '\n')
        dt[strlen(dt) - 1] = '\0';
      file << "[" << dt << "] Match result: ";
    }

    if (winner == 1)
      file << "Player 1 (Green) Won!\n";
    else if (winner == 2)
      file << "Player 2 (Cyan) Won!\n";
    else
      file << "Draw!\n";

    file.close();
  }
}
