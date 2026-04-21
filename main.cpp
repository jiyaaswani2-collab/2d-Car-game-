// simple_2d_racer.c
// Compile: on Windows with Code::Blocks (MinGW + freeglut)
// Linker: -lopengl32 -lglu32 -lfreeglut  (or use the Code::Blocks GUI to add libs)
// On Linux: g++ simple_2d_racer.c -lGL -lGLU -lglut -o racer

#include <GL/glut.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>

#define WINDOW_W 600
#define WINDOW_H 800

// Player car
float playerX = 0.0f;        // center x in normalized coords (-1..1)
const float playerY = -0.7f; // fixed y
const float carW = 0.18f;
const float carH = 0.28f;
float playerSpeed = 0.06f;

// Road lanes
const float roadLeft = -0.6f;
const float roadRight = 0.6f;
const int numRoadLines = 10;
float roadLineOffsets[numRoadLines];

// Obstacles
#define MAX_OBS 6
typedef struct {
    bool active;
    float x, y;    // center pos
    float w, h;
    float speed;
} Obstacle;
Obstacle obstacles[MAX_OBS];

// Game state
bool running = true;
bool crashed = false;
int score = 0;
float gameSpeed = 0.008f; // base speed for obstacles (increases)
int spawnTimer = 0;

// Utility: random float in range
float frand(float a, float b) {
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

void initObstacles() {
    for (int i = 0; i < MAX_OBS; ++i) {
        obstacles[i].active = false;
    }
}

void resetGame() {
    playerX = 0.0f;
    playerSpeed = 0.06f;
    for (int i = 0; i < numRoadLines; ++i) roadLineOffsets[i] = (i * (2.0f / numRoadLines));
    initObstacles();
    score = 0;
    gameSpeed = 0.008f;
    spawnTimer = 0;
    crashed = false;
    running = true;
    srand(time(NULL));
}

void init() {
    glClearColor(0.2f, 0.6f, 0.9f, 1.0f); // sky
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1, 1, -1, 1);
    resetGame();
}

// Draw filled rectangle centered at (cx, cy)
void drawRect(float cx, float cy, float w, float h) {
    float x1 = cx - w/2, x2 = cx + w/2;
    float y1 = cy - h/2, y2 = cy + h/2;
    glBegin(GL_QUADS);
      glVertex2f(x1, y1);
      glVertex2f(x2, y1);
      glVertex2f(x2, y2);
      glVertex2f(x1, y2);
    glEnd();
}

// Draw player car (simple shape)
void drawPlayer() {
    // body
    glColor3f(0.9f, 0.1f, 0.1f);
    drawRect(playerX, playerY, carW, carH);

    // windows
    glColor3f(0.2f, 0.7f, 0.9f);
    drawRect(playerX, playerY + 0.05f, carW*0.6f, carH*0.35f);

    // wheels
    glColor3f(0.05f, 0.05f, 0.05f);
    drawRect(playerX - carW*0.35f, playerY - carH*0.45f, carW*0.3f, carH*0.15f);
    drawRect(playerX + carW*0.35f, playerY - carH*0.45f, carW*0.3f, carH*0.15f);
}

// Draw obstacle car
void drawObstacle(Obstacle* o) {
    if (!o->active) return;
    glColor3f(0.1f, 0.1f, 0.8f);
    drawRect(o->x, o->y, o->w, o->h);
    glColor3f(0.2f, 0.7f, 0.9f);
    drawRect(o->x, o->y + 0.05f, o->w*0.6f, o->h*0.35f);
    glColor3f(0.05f, 0.05f, 0.05f);
    drawRect(o->x - o->w*0.35f, o->y - o->h*0.45f, o->w*0.3f, o->h*0.15f);
    drawRect(o->x + o->w*0.35f, o->y - o->h*0.45f, o->w*0.3f, o->h*0.15f);
}

// Road background and lane markers
void drawRoad() {
    // grass sides
    glColor3f(0.1f, 0.6f, 0.2f);
    glBegin(GL_QUADS);
      glVertex2f(-1.0f, -1.0f);
      glVertex2f(roadLeft, -1.0f);
      glVertex2f(roadLeft, 1.0f);
      glVertex2f(-1.0f, 1.0f);
    glEnd();
    glBegin(GL_QUADS);
      glVertex2f(roadRight, -1.0f);
      glVertex2f(1.0f, -1.0f);
      glVertex2f(1.0f, 1.0f);
      glVertex2f(roadRight, 1.0f);
    glEnd();

    // road
    glColor3f(0.18f, 0.18f, 0.18f);
    glBegin(GL_QUADS);
      glVertex2f(roadLeft, -1.0f);
      glVertex2f(roadRight, -1.0f);
      glVertex2f(roadRight, 1.0f);
      glVertex2f(roadLeft, 1.0f);
    glEnd();

    // side lines
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
      glVertex2f(roadLeft + 0.05f, -1.0f); glVertex2f(roadLeft + 0.05f, 1.0f);
      glVertex2f(roadRight - 0.05f, -1.0f); glVertex2f(roadRight - 0.05f, 1.0f);
    glEnd();

    // center dashed line (moving)
    glLineWidth(6.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < numRoadLines; ++i) {
        float cx = 0.0f;
        float lineLen = 0.12f;
        // offset changes to simulate motion
        float offset = roadLineOffsets[i];
        float y1 = 1.0f - fmod(offset, 2.0f);
        float y2 = y1 - lineLen;
        glVertex2f(cx, y1);
        glVertex2f(cx, y2);
    }
    glEnd();
}

// Spawn obstacle in a free slot
void spawnObstacle() {
    for (int i = 0; i < MAX_OBS; ++i) {
        if (!obstacles[i].active) {
            obstacles[i].active = true;
            // choose lane x (3 lanes)
            int lane = rand() % 3; // 0,1,2
            float laneWidth = (roadRight - roadLeft) / 3.0f;
            float laneCenter = roadLeft + laneWidth/2.0f + lane*laneWidth;
            obstacles[i].x = laneCenter;
            obstacles[i].y = 1.4f + frand(0.0f, 0.6f); // spawn above view
            obstacles[i].w = carW * 0.9f;
            obstacles[i].h = carH * 0.9f;
            obstacles[i].speed = gameSpeed + frand(0.002f, 0.01f);
            break;
        }
    }
}

// AABB collision detection
bool checkCollision(Obstacle* o) {
    if (!o->active) return false;
    float leftA = playerX - carW/2;
    float rightA = playerX + carW/2;
    float topA = playerY + carH/2;
    float bottomA = playerY - carH/2;

    float leftB = o->x - o->w/2;
    float rightB = o->x + o->w/2;
    float topB = o->y + o->h/2;
    float bottomB = o->y - o->h/2;

    if (leftA > rightB || rightA < leftB || topA < bottomB || bottomA > topB) return false;
    return true;
}

void drawText(float x, float y, const char* string) {
    glRasterPos2f(x, y);
    while (*string) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *string);
        ++string;
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // road & background
    drawRoad();

    // road line offsets update (visual only)
    // (these are updated in update function, but safe to reflect here)

    // obstacles
    for (int i = 0; i < MAX_OBS; ++i) {
        drawObstacle(&obstacles[i]);
    }

    // player
    drawPlayer();

    // HUD
    char buf[64];
    sprintf(buf, "Score: %d", score);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawText(-0.95f, 0.9f, buf);

    if (crashed) {
        glColor3f(1.0f, 0.0f, 0.0f);
        drawText(-0.15f, 0.0f, "CRASH! Press 'r' to restart");
    }

    glutSwapBuffers();
}

// Update function called by timer
void update(int value) {
    if (!running) return;

    if (!crashed) {
        // Move road lines
        for (int i = 0; i < numRoadLines; ++i) {
            roadLineOffsets[i] += gameSpeed * 30.0f; // scale for good visual
            if (roadLineOffsets[i] > 2.5f) roadLineOffsets[i] -= 2.5f;
        }

        // Update obstacles
        for (int i = 0; i < MAX_OBS; ++i) {
            if (obstacles[i].active) {
                obstacles[i].y -= obstacles[i].speed + gameSpeed * 0.5f;
                // off screen -> deactivate & increase score
                if (obstacles[i].y < -1.4f) {
                    obstacles[i].active = false;
                    score += 10;
                } else {
                    // collision?
                    if (checkCollision(&obstacles[i])) {
                        crashed = true;
                        running = false;
                    }
                }
            }
        }

        // spawn logic
        spawnTimer++;
        if (spawnTimer > 60) { // about 1 second at 60fps
            if (rand() % 100 < 40) spawnObstacle(); // 40% chance
            spawnTimer = 0;
        }

        // slowly increase speed with score
        if (score % 100 == 0 && score > 0) {
            gameSpeed += 0.0007f;
        }

        // ensure player stays inside road bounds
        float leftBound = roadLeft + carW/2 + 0.02f;
        float rightBound = roadRight - carW/2 - 0.02f;
        if (playerX < leftBound) playerX = leftBound;
        if (playerX > rightBound) playerX = rightBound;
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0); // ~60 FPS
}

// Keyboard handlers
void keyboard(unsigned char key, int x, int y) {
    if (key == 27) exit(0); // ESC
    if (key == 'r' || key == 'R') {
        resetGame();
    }
}

void specialKeys(int key, int x, int y) {
    if (crashed) return;
    float laneWidth = (roadRight - roadLeft) / 3.0f;
    if (key == GLUT_KEY_LEFT) {
        // move one lane left
        float leftBound = roadLeft + carW/2 + 0.02f;
        playerX -= laneWidth;
        if (playerX < leftBound) playerX = leftBound;
    }
    if (key == GLUT_KEY_RIGHT) {
        // move one lane right
        float rightBound = roadRight - carW/2 - 0.02f;
        playerX += laneWidth;
        if (playerX > rightBound) playerX
