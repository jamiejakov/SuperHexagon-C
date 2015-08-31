//
//  main.c
//  SuperHexagon
//
//  Created by Vadims Brodskis on 6/21/14.
//  Copyright (c) 2014 Vadims Brodskis. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <GLUT/glut.h>
#include <string.h>
//#include <windows.h>
#define _CRT_SECURE_NO_WARNINGS

#define VERSION "1.1"
#define CHAR_SIZE 0.013
#define CHAR_SPEED 12.2
#define PLAYER_OFFSET 0.023

#define INNER_HEXAGON_H 0.08
#define STARTING_DISTANCE 1.5

#define WIN_CONDITION 10
#define SCREEN_ROTATION_LIMIT 150

// ------------------------- Global Variables -------------------------

const GLfloat colChar[3] = { 0.99, 0.39, 0.0 }; // character default color
const double charSpeed[2] = { -CHAR_SIZE*0.05, CHAR_SIZE*0.05 }; //character default size
//FILE *fp;

int menu = 0; //different screens: 0-Welcome, 1-LevelSelect, 2-GameOver, 3-PlayGame, 4-Win
int startup = 0; // 0-level startup, 1-allsetup, game in progress.
int gameOver = 0; // 0-game not over, 1-game over
int win = 0; // 0-game not won, 1-game won
int level = 1;        // 1 = HARD, 2 = HARDER, 3 = HARDEST
time_t start;         // timer for the game itself
double secondsSinceStart = 0.0; // =current time - start time
int hexagonTimer = 0;   // when new hexagon wall should appear
int highScore = 0;  // top score, not yet implemented
int keyLeft = 0, keyRight = 0;
double screenRotate = 0; //rotation degrees
double screenRotateDirection = 1; // 1 - rotate left, -1 rotate right
double wallSpeed = 0.01; // speed of the wall spinning
double screenRotation = 0.6; // speed of rotation (this * direction) added to Rotate
int hexagonAppearTime = 30; // after how much time a new set of walls should appear
GLfloat levelColor[3]; //color of the level background

struct player{
    double pos[2];
    double realPos[2];
    double degree;  // angle of player in regards to rotation on Z axis
    double direction[2];
    double size;
    int touchedWall;
    GLfloat color[3];
    double speed;
};
struct wall{
    double pos[2];
    double limit[2];
    double direct[2];
    GLfloat color[3];
    double size;    // a of a hexagon (side of a single equaliteral triangle)
    double h;       // the height of the equliteral triangle (distance between center and point 0,0);
    double l;       // the distance between the center of the side and the points on the X axis
    double h2;      // the distance between the center of the side and the points on the Y axis
    int location;   // top=1, top-right=2, bottom-right=3, bottom=4, bottom-left=5, top-left=6
    int type;       // small or large (0 = small, 1 = large)
    double speed;   // speed of the walls closing in on the center
    int speedInc;   // how much the speed is increased when coming close to the center
    struct wall *next;
    struct wall *prev;
};
struct textBox{
    double boxPos[2];
    double txtPos[2];
    GLfloat bgColor[3];
    GLfloat txtColor[3];
    double size[2];
    char text[30];
};

struct player *player1 = NULL;
struct wall *newWall = NULL;
struct wall *rootWall = NULL;
struct textBox *escBox = NULL;
struct textBox *spaceBox = NULL;

// ------------------------- TextBoxes -------------------------

void drawRect(double pos[], double size[]){
    double x = pos[0], y = pos[1];
    double width = size[0], height = size[1];
    glBegin(GL_POLYGON);
    glVertex2d(x, y);
    glVertex2d(x + width, y);
    glVertex2d(x + width, y - height);
    glVertex2d(x, y - height);
    glEnd();
}
void drawTextBoxes(){
    struct textBox *tmp[2];
    int boxes = 0;
    int i;
    int j;
    unsigned long slen = 0;
    if (escBox != NULL){
        boxes++;
        tmp[0] = escBox;
    }
    if (spaceBox != NULL){
        boxes++;
        tmp[1] = spaceBox;
    }
    
    for (i = 0; i<boxes; i++){
        glColor3d(tmp[i]->bgColor[0], tmp[i]->bgColor[1], tmp[i]->bgColor[2]);
        drawRect(tmp[i]->boxPos, tmp[i]->size);
        glColor3d(tmp[i]->txtColor[0], tmp[i]->txtColor[1], tmp[i]->txtColor[2]);
        glRasterPos3f(tmp[i]->txtPos[0], tmp[i]->txtPos[1], 0);
        for (j = 0; j<strlen(tmp[i]->text); j++){
            glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)tmp[i]->text[j]);
        }
    }
}

// ------------------------- Player -------------------------

void drawTri(double point1x, double point1y, double point2x,
             double point2y, double point3x, double point3y){
    glBegin(GL_POLYGON);
    glVertex2d(point1x, point1y);
    glVertex2d(point2x, point2y);
    glVertex2d(point3x, point3y);
    glEnd();
}
void drawChar(double posGiven[2], double size){
    glColor3d(colChar[0], colChar[1], colChar[2]);
    drawTri(posGiven[0] + size, posGiven[1] - size, posGiven[0] - size, posGiven[1] - size, posGiven[0], posGiven[1] + size);
}
void crushingWall(){
    struct wall *e = rootWall;
    if (e != NULL){
        while (e != NULL){
            if (sqrt(pow(player1->realPos[0] - e->pos[0], 2.0) + pow(player1->realPos[1] - e->pos[1], 2.0)) <= player1->size * 2){
                player1->touchedWall++;
            }
            e = e->next;
        }
    }
}

// ------------------------- Hexagons / Walls -------------------------

int angle(const int loc, const int startEnd){
    int result = 0;
    switch (loc){
        case 2:
        case 5:
            result = -1;
            break;
        case 3:
        case 6:
            result = +1;
            break;
        default:
            result = 0;
    }
    return result*startEnd;
}
void drawLine(double posStart[2], double posEnd[2]){
    
    glBegin(GL_LINE_LOOP);
    glVertex2d(posStart[0], posStart[1]);
    glVertex2d(posEnd[0], posEnd[1]);
    glEnd();
}
void drawWall(struct wall *e){
    double startPos[2];
    double endPos[2];
    startPos[0] = e->pos[0] - e->l;
    endPos[0] =   e->pos[0] + e->l;
    startPos[1] = e->pos[1] + e->h2 * angle(e->location, -1);
    endPos[1] =   e->pos[1] + e->h2 * angle(e->location, 1);
    glColor3d(e->color[0], e->color[1], e->color[2]);
    drawLine(startPos, endPos);
    if (e->next != NULL){
        drawWall(e->next);
    }
}
void insertWall(struct wall *p){
    struct wall *curE;
    if (rootWall == NULL){
        rootWall = p;
        return;
    }
    curE = rootWall;
    while (curE->next != NULL)
        curE = curE->next;
    curE->next = p;
    p->prev = curE;
}
void deleteWall(struct wall *p){
    if (p != NULL){
        if (p == rootWall){
            rootWall = p->next;
            if (rootWall != NULL){
                rootWall->prev = NULL;
            }
        }
        else if (p->next == NULL){
            p->prev->next = NULL;
        }
        else {
            p->prev->next = p->next;
            p->next->prev = p->prev;
        }
        free(p);
    }
}

void calculateWallParam(struct wall *w){
    double x = w->pos[0];
    double y = w->pos[1];
    
    w->h = sqrt(pow(x, 2) + pow(y, 2));
    if (w->type == 1){
        w->size = w->h*(2 * sqrt(3) / 3);
    }
    
    switch (w->location){
        case 1:
        case 4:
            w->l = w->size / 2;
            w->h2 = 0;
            break;
        case 2:
        case 3:
        case 5:
        case 6:
            w->l = w->size / 4;
            w->h2 = w->size*(sqrt(3) / 4);
            break;
        default:
            break;
    }
    
    
}

void createWalls(const int number, const int locs[]){
    int i = 1;
    double pos[2], dir[2];
    double offset = 1;
    int direct = 1;
    double speed = wallSpeed;
    srand((unsigned)time(NULL));
    for (i = 0; i<number; i++){
        newWall = (struct wall *)malloc(sizeof(struct wall));
        newWall->location = locs[i];
        switch (locs[i]){
            case 1:
                pos[0] = 0.0;
                pos[1] = 1.0;
                dir[0] = 0.0;
                dir[1] = -2.0;
                break;
            case 2:
                pos[0] = sqrt(3) / 2;
                pos[1] = 0.5;
                dir[0] = -1.0;
                dir[1] = -1.0;
                break;
            case 3:
                pos[0] = sqrt(3) / 2;
                pos[1] = -0.5;
                dir[0] = -1.0;
                dir[1] = 1.0;
                break;
            case 4:
                pos[0] = 0.0;
                pos[1] = -1.0;
                dir[0] = 0.0;
                dir[1] = 2.0;
                break;
            case 5:
                pos[0] = -sqrt(3) / 2;
                pos[1] = -0.5;
                dir[0] = 1.0;
                dir[1] = 1.0;
                break;
            case 6:
                pos[0] = -sqrt(3) / 2;
                pos[1] = 0.5;
                dir[0] = 1.0;
                dir[1] = -1.0;
                break;
            default:
                break;
        }
        if (startup > 0){
            offset = INNER_HEXAGON_H;
            direct = 0;
            speed = 0;
        }
        else{
            offset = STARTING_DISTANCE;
        }
        newWall->pos[0] = pos[0] * offset;
        newWall->pos[1] = pos[1] * offset;
        newWall->direct[0] = dir[0] * direct;
        newWall->direct[1] = dir[1] * direct;
        newWall->type = 1;
        newWall->speed = speed;
        newWall->speedInc = 0;
        newWall->color[0] = levelColor[0];
        newWall->color[1] = levelColor[1];
        newWall->color[2] = levelColor[2];
        newWall->limit[0] = newWall->pos[0] * INNER_HEXAGON_H / STARTING_DISTANCE;
        newWall->limit[1] = newWall->pos[1] * INNER_HEXAGON_H / STARTING_DISTANCE;
        calculateWallParam(newWall);
        if (startup > 0){
            newWall->type = 0;
        }
        
        newWall->next = NULL;
        newWall->prev = NULL;
        insertWall(newWall);
    }
}

int wallRepeats(int num, int locs[], int n){
    int i = 0;
    for (i = 0; i<num; i++){
        if (locs[i] == n){
            return 0;
        }
    }
    return 1;
}
void addNewHexagon(){
    const int number = (int)rand() % 2 + 4;
    int locs[6];
    int tmp = (int)rand() % 6 + 1;
    int i = 1;
    locs[0] = tmp;
    while (i < number){
        tmp = (int)rand() % 6 + 1;
        if (wallRepeats(i + 1, locs, tmp)){
            locs[i] = tmp;
            i++;
        }
    }
    createWalls(number, locs);
    
}

// ------------------------- Game Setup -------------------------

void clearGame(){
    struct wall *w = rootWall;
    while (w != NULL){
        deleteWall(w);
        w = rootWall;
    }
}

void setupSmallHexagon(){
    const int smallHexagon[] = { 1, 2, 3, 4, 5, 6 };
    startup = 1;
    createWalls(6, smallHexagon);
    startup = 0;
}
void setupPlayer(){
    player1 = (struct player *)malloc(sizeof(struct player));
    player1->size = CHAR_SIZE;
    player1->speed = CHAR_SPEED;
    player1->pos[0] = 0;
    player1->pos[1] = INNER_HEXAGON_H + player1->size + PLAYER_OFFSET;
    player1->realPos[0] = player1->pos[0];
    player1->realPos[1] = player1->pos[1];
    player1->touchedWall = 0;
    player1->degree = 0;
}
void setupLevel(){
    switch (level){
        case 1:
            break;
        case 2:
            wallSpeed *= (level*0.7);
            player1->speed *= (level*0.5);
            screenRotation *= (level*0.6);
            hexagonAppearTime = 18;
            break;
        case 3:
            wallSpeed *= (level*0.8);
            player1->speed *= (level*0.6);
            screenRotation *= (level*0.7);
            hexagonAppearTime = 15;
            break;
        default:
            break;
    }
}
void startNewGame(){
    gameOver = 0;
    win = 0;
    startup = 0;
    keyLeft = 0;
    keyRight = 0;
    hexagonTimer = 0;
    start = time(0);
    secondsSinceStart = 0;
    menu = 3;
    hexagonAppearTime = 30;
    screenRotation = 0.6;
    wallSpeed = 0.01;
    
    screenRotate = 0;
    screenRotateDirection = (int)rand() % 2;
    if (screenRotateDirection == 0){
        screenRotateDirection = -1;
    }
    
    
    srand((unsigned)time(NULL));
    setupPlayer();
    setupLevel();
    setupSmallHexagon();
    addNewHexagon();
}

// ------------------------- MENU -------------------------

void showWelcomeScreen(){
    int i;
    char c[50];
    char n[] = "Welcome to ULTRAHEXAGON v";
    char k[] = "Use Left and Right keys to move, avoid the walls";
    
    clearGame();
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glColor3d(1.0, 1.0, 1.0);
    sprintf(n, VERSION);
    
    glRasterPos3f(-0.3, 0.6, 0);
    for (i = 0; i<strlen(n); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)n[i]);
    }
    glRasterPos3f(-0.5, 0.1, 0);
    sprintf(c, "Clear the level by surviving %d SECONDS!", WIN_CONDITION);
    for (i = 0; i<strlen(c); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)c[i]);
    }
    glRasterPos3f(-0.6, 0.0, 0);
    for (i = 0; i<strlen(k); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)k[i]);
    }
    menu = 0;
    
    escBox = (struct textBox *)malloc(sizeof(struct textBox));
    escBox->bgColor[0] = 1.0;
    escBox->bgColor[1] = 0.0;
    escBox->bgColor[2] = 0.0;
    escBox->txtColor[0] = 0.0;
    escBox->txtColor[1] = 0.0;
    escBox->txtColor[2] = 0.0;
    sprintf(escBox->text, "EXIT");
    escBox->size[0] = 0.14;
    escBox->size[1] = 0.08;
    escBox->boxPos[0] = -1.0;
    escBox->boxPos[1] = 1.0;
    escBox->txtPos[0] = escBox->boxPos[0] + 0.02;
    escBox->txtPos[1] = escBox->boxPos[1] - 0.05;
    
    spaceBox = (struct textBox *)malloc(sizeof(struct textBox));
    spaceBox->bgColor[0] = 0.0;
    spaceBox->bgColor[1] = 0.0;
    spaceBox->bgColor[2] = 1.0;
    spaceBox->txtColor[0] = 1.0;
    spaceBox->txtColor[1] = 1.0;
    spaceBox->txtColor[2] = 1.0;
    sprintf(spaceBox->text, "SPACE TO START");
    spaceBox->size[0] = 0.40;
    spaceBox->size[1] = 0.08;
    spaceBox->boxPos[0] = -spaceBox->size[0] / 2;
    spaceBox->boxPos[1] = -0.2;
    spaceBox->txtPos[0] = spaceBox->boxPos[0] + 0.02;
    spaceBox->txtPos[1] = spaceBox->boxPos[1] - 0.05;
    drawTextBoxes();
    
}
void showLevelSelect(){
    int i;
    char sl[] = "Select Level";
    char l[20];
    char k[] = "<                                                     >";
    menu = 1;
    
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glColor3d(1.0, 1.0, 1.0);
    glRasterPos3f(-0.16, 0.7, 0);
    for (i = 0; i<strlen(sl); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)sl[i]);
    }
    glColor3d(levelColor[0], levelColor[1], levelColor[2]);
    switch (level){
        case 1:
            sprintf(l, "1:NORMAL");
            levelColor[0] = 0.9;
            levelColor[1] = 0.0;
            levelColor[2] = 0.0;
            glRasterPos3f(-0.10, 0.4, 0);
            break;
        case 2:
            sprintf(l, "2:HARD");
            levelColor[0] = 0.0;
            levelColor[1] = 0.0;
            levelColor[2] = 0.9;
            glRasterPos3f(-0.08, 0.4, 0);
            break;
        case 3:
            sprintf(l, "3:EXTREME");
            levelColor[0] = 0.0;
            levelColor[1] = 0.9;
            levelColor[2] = 0.0;
            glRasterPos3f(-0.11, 0.4, 0);
            break;
        default:
            break;
    }
    glColor3d(levelColor[0], levelColor[1], levelColor[2]);
    for (i = 0; i<strlen(l); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)l[i]);
    }
    glRasterPos3f(-0.7, -0.0, 0);
    for (i = 0; i<strlen(k); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)k[i]);
    }
    
    escBox = (struct textBox *)malloc(sizeof(struct textBox));
    escBox->bgColor[0] = 0.69;
    escBox->bgColor[1] = 0.69;
    escBox->bgColor[2] = 0.69;
    escBox->txtColor[0] = 0.0;
    escBox->txtColor[1] = 0.0;
    escBox->txtColor[2] = 0.0;
    sprintf(escBox->text, "BACK");
    escBox->size[0] = 0.14;
    escBox->size[1] = 0.08;
    escBox->boxPos[0] = -1.0;
    escBox->boxPos[1] = 1.0;
    escBox->txtPos[0] = escBox->boxPos[0] + 0.02;
    escBox->txtPos[1] = escBox->boxPos[1] - 0.05;
    
    spaceBox = (struct textBox *)malloc(sizeof(struct textBox));
    spaceBox->bgColor[0] = levelColor[0];
    spaceBox->bgColor[1] = levelColor[1];
    spaceBox->bgColor[2] = levelColor[2];
    spaceBox->txtColor[0] = 1.0;
    spaceBox->txtColor[1] = 1.0;
    spaceBox->txtColor[2] = 1.0;
    sprintf(spaceBox->text, "SPACE TO SELECT");
    spaceBox->size[0] = 0.43;
    spaceBox->size[1] = 0.08;
    spaceBox->boxPos[0] = -spaceBox->size[0] / 2;
    spaceBox->boxPos[1] = -0.2;
    spaceBox->txtPos[0] = spaceBox->boxPos[0] + 0.02;
    spaceBox->txtPos[1] = spaceBox->boxPos[1] - 0.05;
    drawTextBoxes();
    
}
void showGameOverScreen(){
    int i;
    char go[] = "GAME OVER!";
    char sec[40];
    char ret[7] = "RETRY?";
    char hr[10];
    menu = 2;
    clearGame();
    
    switch (level){
        case 1:
            sprintf(hr, "NORMAL");
            break;
        case 2:
            sprintf(hr, "HARD");
            break;
        case 3:
            sprintf(hr, "EXTREME");
            break;
        default:
            break;
    }
    
    glClearColor(0.4, 0.4, 0.4, 0.0);
    glColor3d(levelColor[0], levelColor[1], levelColor[2]);
    
    glRasterPos3f(-0.11, 0.1, 0);
    for (i = 0; i<strlen(go); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)go[i]);
    }
    sprintf(sec, "Last run - %.2f seconds on %s", secondsSinceStart, hr);
    glRasterPos3f(-0.37, 0.0, 0);
    for (i = 0; i<strlen(sec); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)sec[i]);
    }
    glRasterPos3f(-0.07, -0.1, 0);
    for (i = 0; i<strlen(ret); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)ret[i]);
    }
    
    escBox = (struct textBox *)malloc(sizeof(struct textBox));
    escBox->bgColor[0] = 0.0;
    escBox->bgColor[1] = 0.0;
    escBox->bgColor[2] = 0.0;
    escBox->txtColor[0] = 1.0;
    escBox->txtColor[1] = 1.0;
    escBox->txtColor[2] = 1.0;
    sprintf(escBox->text, "MENU");
    escBox->size[0] = 0.14;
    escBox->size[1] = 0.08;
    escBox->boxPos[0] = -1.0;
    escBox->boxPos[1] = 1.0;
    escBox->txtPos[0] = escBox->boxPos[0] + 0.02;
    escBox->txtPos[1] = escBox->boxPos[1] - 0.05;
    
    spaceBox = (struct textBox *)malloc(sizeof(struct textBox));
    spaceBox->bgColor[0] = levelColor[0];
    spaceBox->bgColor[1] = levelColor[1];
    spaceBox->bgColor[2] = levelColor[2];
    spaceBox->txtColor[0] = 1.0;
    spaceBox->txtColor[1] = 1.0;
    spaceBox->txtColor[2] = 1.0;
    sprintf(spaceBox->text, "YES");
    spaceBox->size[0] = 0.125;
    spaceBox->size[1] = 0.08;
    spaceBox->boxPos[0] = -spaceBox->size[0] / 2;
    spaceBox->boxPos[1] = -0.2;
    spaceBox->txtPos[0] = spaceBox->boxPos[0] + 0.02;
    spaceBox->txtPos[1] = spaceBox->boxPos[1] - 0.05;
    drawTextBoxes();
    
}
void showWinScreen(){
    int i;
    char c[40];
    char l[10];
    menu = 4;
    clearGame();
    
    glColor3d(1.0, 1.0, 1.0);
    glClearColor(0.0, 0.5, 0.0, 0.0);
    
    switch (level){
        case 1:
            sprintf(l, "NORMAL");
            break;
        case 2:
            sprintf(l, "HARD");
            break;
        case 3:
            sprintf(l, "EXTREME");
            break;
        default:
            break;
    }
    
    sprintf(c, "%s level clear! Congratulations", l);
    glRasterPos3f(-0.45, 0, 0);
    for (i = 0; i<strlen(c); i++){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)c[i]);
    }
    
    escBox = (struct textBox *)malloc(sizeof(struct textBox));
    escBox->bgColor[0] = 0.0;
    escBox->bgColor[1] = 0.0;
    escBox->bgColor[2] = 0.0;
    escBox->txtColor[0] = 1.0;
    escBox->txtColor[1] = 1.0;
    escBox->txtColor[2] = 1.0;
    sprintf(escBox->text, "MENU");
    escBox->size[0] = 0.35;
    escBox->size[1] = 0.08;
    escBox->boxPos[0] = -1.0;
    escBox->boxPos[1] = 1.0;
    escBox->txtPos[0] = escBox->boxPos[0] + 0.02;
    escBox->txtPos[1] = escBox->boxPos[1] - 0.05;
    
    spaceBox = (struct textBox *)malloc(sizeof(struct textBox));
    spaceBox->bgColor[0] = 0.0;
    spaceBox->bgColor[1] = 0.0;
    spaceBox->bgColor[2] = 0.0;
    spaceBox->txtColor[0] = 1.0;
    spaceBox->txtColor[1] = 1.0;
    spaceBox->txtColor[2] = 1.0;
    sprintf(spaceBox->text, "BACK TO LEVEL SELECT");
    spaceBox->size[0] = 0.55;
    spaceBox->size[1] = 0.08;
    spaceBox->boxPos[0] = -spaceBox->size[0] / 2;
    spaceBox->boxPos[1] = -0.2;
    spaceBox->txtPos[0] = spaceBox->boxPos[0] + 0.02;
    spaceBox->txtPos[1] = spaceBox->boxPos[1] - 0.05;
    drawTextBoxes();
}

// ------------------------- Keyboard -------------------------

void checkSpecialKeyPressed(int key, int x, int y){
    switch (key){
        case GLUT_KEY_LEFT:
            keyLeft = 1;
            if (menu == 1){
                if (level == 1){
                    level = 3;
                }
                else
                    level--;
            }
            break;
        case GLUT_KEY_RIGHT:
            keyRight = 1;
            if (menu == 1){
                if (level == 3){
                    level = 1;
                }
                else
                    level++;
            }
            break;
    }
}
void checkSpecialKeyReleased(int key, int x, int y){
    switch (key){
        case GLUT_KEY_LEFT: keyLeft = 0; break;
        case GLUT_KEY_RIGHT: keyRight = 0; break;
    }
}
void checkKeyPressed(unsigned char key, int x, int y){
    switch (key){
        case ' ':
            if (menu == 4){
                menu = 1;
            }
            else if (menu == 1){
                startNewGame();
            }
            else if (menu == 2){
                startNewGame();
            }
            else if (menu == 0){
                menu++;
            }
            break;
        case 127:
            
            break;
        case 27:
            if (menu == 0){
                exit(0);
            }
            else if (menu == 3){
                menu = 1;
                clearGame();
            }
            else if (menu == 4){
                menu = 1;
            }
            else{
                menu--;
            }
            break;
    }
}

// ------------------------- Updating Positions -------------------------

double convertToPositive(double d){
    double result = d;
    if (result <= 0){
        result *= -1;
    }
    return result;
}
void updateWallPos(struct wall *w){
    if (w != NULL){
        if (sqrt(pow(0.0 - w->pos[0], 2.0) + pow(0.0 - w->pos[1], 2.0)) <= player1->size * 15 && w->speedInc != 1){
            w->speed *= 1.5;
            w->speedInc = 1;
        }
        w->pos[0] += w->direct[0] * w->speed;
        w->pos[1] += w->direct[1] * w->speed*(sqrt(3) / 3);
        calculateWallParam(w);
        if (w->next != NULL){
            updateWallPos(w->next);
        }
        if (convertToPositive(w->pos[0]) <= convertToPositive(w->limit[0]) &&
            convertToPositive(w->pos[1]) <= convertToPositive(w->limit[1]) &&
            w->type != 0){
            deleteWall(w);
        }
    }
}
void changePlayerRealPos(struct player *p){
    int deg = p->degree;
    double x, y;
    double h = INNER_HEXAGON_H + player1->size + PLAYER_OFFSET;
    if ((deg >= -30 && deg <= 30) || deg <= -330 || deg >= 330){               // 1
        x = 0;
        y = h;
    }
    else if ((deg > 270 && deg < 330) || (deg > -90 && deg < -30)){            // 2
        x = h*sqrt(3) / 2;
        y = h / 2;
    }
    else if ((deg > 210 && deg < 270) || (deg > -150 && deg < -90)){           // 3
        x = h*sqrt(3) / 2;
        y = -h / 2;
    }
    else if ((deg >= 150 && deg <= 210) || (deg >= -210 && deg <= -150)){     // 4
        x = 0;
        y = -h;
    }
    else if ((deg > 90 && deg < 150) || (deg > -270 && deg < -210)){           // 5
        x = -h*sqrt(3) / 2;
        y = -h / 2;
    }
    else if ((deg > 30 && deg < 90) || (deg > -330 && deg < -270)){            // 6
        x = -h*sqrt(3) / 2;
        y = h / 2;
    }
    p->realPos[0] = x;
    p->realPos[1] = y;
    
}
double fixDegrees(double deg){
    double result = deg;
    if (result >= 360){
        result = deg - 360.0;
    }
    else if (deg <= -360){
        result = deg + 360;
    }
    return result;
}
void updatePos(int value){
    if (menu == 3){
        if (rootWall != NULL){
            updateWallPos(rootWall);
        }
        if (hexagonTimer >= hexagonAppearTime){
            addNewHexagon();
            hexagonTimer = 0;
        }
        if (keyLeft){
            player1->degree = fixDegrees(player1->degree += player1->speed);
            //player1->direction[0] = -1;
            //player1->direction[1] = 0;
            changePlayerRealPos(player1);
        }
        if (keyRight){
            player1->degree = fixDegrees(player1->degree -= player1->speed);
            //player1->direction[0] = 1;
            //player1->direction[1] = 0;
            changePlayerRealPos(player1);
        }
        hexagonTimer++;
        player1->touchedWall = 0;
        crushingWall();
    }
    glutPostRedisplay();
    glutTimerFunc(30, updatePos, 0);
}

// ------------------------- Display -------------------------

void displayGameText(){
    
    secondsSinceStart = difftime(time(0), start);
    
    escBox = (struct textBox *)malloc(sizeof(struct textBox));
    escBox->bgColor[0] = 0.69;
    escBox->bgColor[1] = 0.69;
    escBox->bgColor[2] = 0.69;
    escBox->txtColor[0] = 0.0;
    escBox->txtColor[1] = 0.0;
    escBox->txtColor[2] = 0.0;
    sprintf(escBox->text, "MENU");
    escBox->size[0] = 0.14;
    escBox->size[1] = 0.08;
    escBox->boxPos[0] = -1.0;
    escBox->boxPos[1] = 1.0;
    escBox->txtPos[0] = escBox->boxPos[0] + 0.02;
    escBox->txtPos[1] = escBox->boxPos[1] - 0.05;
    
    spaceBox = (struct textBox *)malloc(sizeof(struct textBox));
    spaceBox->bgColor[0] = 0.69;
    spaceBox->bgColor[1] = 0.69;
    spaceBox->bgColor[2] = 0.69;
    spaceBox->txtColor[0] = 0.0;
    spaceBox->txtColor[1] = 0.0;
    spaceBox->txtColor[2] = 0.0;
    sprintf(spaceBox->text, "Time: %.2f", secondsSinceStart);
    spaceBox->size[0] = 0.35;
    spaceBox->size[1] = 0.08;
    spaceBox->boxPos[0] = 1.0 - spaceBox->size[0];
    spaceBox->boxPos[1] = 1.0;
    spaceBox->txtPos[0] = spaceBox->boxPos[0] + 0.02;
    spaceBox->txtPos[1] = spaceBox->boxPos[1] - 0.05;
    
    drawTextBoxes();
}
void displayPlayer(){
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glRotatef(screenRotate + player1->degree, 0.0f, 0.0f, 1.0f);
    drawChar(player1->pos, player1->size);
    glPopMatrix();
}
void display(void){
    glClear(GL_COLOR_BUFFER_BIT);
    if (menu == 0){
        showWelcomeScreen();
    }
    else if (menu == 1){
        showLevelSelect();
    }
    else if (gameOver){
        showGameOverScreen();
    }
    else if (win){
        showWinScreen();
    }
    else{
        if (player1->touchedWall){
            gameOver = 1;
            return;
        }
        if (secondsSinceStart >= WIN_CONDITION){
            win = 1;
            return;
        }
        glClearColor(0.0, 0.0, 0.0, 1.0);
        if (rootWall != NULL){
            if (screenRotate >= SCREEN_ROTATION_LIMIT ||
                screenRotate <= -SCREEN_ROTATION_LIMIT){
                screenRotateDirection *= -1;
            }
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glPushMatrix();
            glRotatef(screenRotate += screenRotation*screenRotateDirection, 0.0f, 0.0f, 1.0f);
            drawWall(rootWall);
            displayPlayer();
            glPopMatrix();
            
            displayGameText();
        }
    }
    glutSwapBuffers();
}

// ------------------------- Init Main -------------------------

void init(void){
    glClearColor(0.0, 0.0, 0.0, 1.0);
}
int main(int argc, char *argv[]){
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(700, 700);
    glutCreateWindow(argv[0]);
    
    
    glutDisplayFunc(display);
    glutKeyboardFunc(checkKeyPressed);
    glutSpecialFunc(checkSpecialKeyPressed);
    glutSpecialUpFunc(checkSpecialKeyReleased);
    
    menu = 0;
    glClearColor(0.0, 0.0, 0.0, 1.0);
    init();
    glutTimerFunc(30, updatePos, 0);
    glutMainLoop();
    
    return EXIT_SUCCESS;
}

// ------------------------- EOF -------------------------