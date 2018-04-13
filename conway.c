/*  Conway's Game of Life
 *  Rules:
 *         1. Any live cell with < 2 neighbors dies
 *         2. Any live cell with 2/3 neighbors lives
 *         3. Any live cell with > 3 neighbors dies
 *         4. Any dead cell with   3 neighbors becomes live
 *  To draw - blit a grey background, then blit white (dead) cells, then black (live) cells
 *  Left click makes a cell live, and right click kills the cell
 *  Spacebar pauses the simulation
 *  Up and down arrows change the speed of the simulation
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include "SDL2/include/SDL.h"

/* All of these are measured in pixels, except ROWS and COLS */
int ROWS = 80;      /* ROWS are vertical,   so # of ROWS = height of board */
int COLS = 80;      /* COLS are horizontal, so # of COLS = width  of board */
int WINDOW_W = 800;
int WINDOW_H = 800;
int BORDER   = 1 * 2; /* ALWAYS make this divisible by 2 */
int CELL_W = 0;
int CELL_H = 0;

SDL_Window  *window;
SDL_Surface *windowSurface;

SDL_Surface *greySurface;
SDL_Surface *blackSurface;
SDL_Surface *whiteSurface;

/* size [COLS * ROWS] */
/* Simplified array, instead of 2d, it's only 1d - index[x][y] is found using a function */
SDL_Rect *gameBoardRects;
int      *gameBoard;
int      *tempBoard;
int boardSize = 0; /* ROWS * COLS */

int getArrValue(int *arr, int x, int y, int w, int h);
void setArr(int **arr, int x, int y, int w, int h, int val);

void init();
void deinit();

int* allocateBoard(int w, int h); 
void freeBoard(int **board); /* board is a pointer to the array */
void initRects();

void draw();
void updateInputs();
int getInput(int key); /* Returns 1 if the key was being pressed down on this frame */
int getFirstInput(int key); /* Returns 1 for rising edge of button press */
int getLastInput(int key); /* Returns 1 for falling edge of button press */

float speed = (1.0 / 2.0) * 1000.0; /* Measured in ms - delay between ticks */
/* tick, then delay for 1 / speed. */

struct Point_
{
    int x, y;
} Mouse, MouseRect;
/* Mouse is the current pixel-position of the mouse */
/* MouseRect is the board position (0 <= x < COLS, 0 <= y < ROWS) of the mouse cursor */

enum inputs { SPACE, L_CLICK, R_CLICK, UP_ARROW, DOWN_ARROW, ENTER, S, NUM_INPUTS };
int currInputs[NUM_INPUTS];
int prevInputs[NUM_INPUTS];

int countNeighbors(int **board, int x, int y); /* Returns # of neighbors including diagonal */
void processInputs();

int paused = 1;
int going = 1;

void tick();

int main(int argc, char *argv[])
{
    SDL_Event e; /* Input event */
    int currTime, prevTime, passTime, lag = 0;
    int i;
    init();
    prevTime = SDL_GetTicks();
    
    while (going)
    {
        currTime = SDL_GetTicks();
        passTime = currTime - prevTime;
        lag += passTime;
        
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                going = 0;
                break;
            }
            updateInputs(&e);
            /* This does stuff like make mouse click affect cells */
            /* It should call draw() aftera click */
        }
        
        processInputs();
        /* Set prev array to be what the curr array is */
        memcpy(prevInputs, currInputs, NUM_INPUTS * sizeof(int));
        
        /* Put this in a check with time to call it every (1 / speed) seconds */
        while (lag >= speed)
        {
            if (!paused)
                tick();
            lag -= speed;
        }
        
        draw();
        prevTime = currTime;
    }
    
    deinit();
    return 0;
}


void init()
{
    int i;
    CELL_W = WINDOW_W / COLS;
    CELL_H = WINDOW_H / ROWS;
    boardSize = ROWS * COLS;
    if (SDL_Init(SDL_INIT_VIDEO))
    {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return;
    }
    window = SDL_CreateWindow("Conway's Game of Life", 
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WINDOW_W,
                              WINDOW_H,
                              SDL_WINDOW_SHOWN/* | SDL_WINDOW_RESIZABLE*/);
    windowSurface = SDL_GetWindowSurface(window);
    gameBoard = allocateBoard(COLS, ROWS);
    tempBoard = allocateBoard(COLS, ROWS);
    for (i = 0; i < NUM_INPUTS; ++i)
    {
        currInputs[i] = 0;
        prevInputs[i] = 0;
    }
    initRects();
    /* Make the single color surfaces */
    /* First, make the surfaces. Then fill them with a color */
    whiteSurface = SDL_CreateRGBSurface(0, 1, 1, 32, 0, 0, 0, 0);
    blackSurface = SDL_CreateRGBSurface(0, 1, 1, 32, 0, 0, 0, 0);
    greySurface  = SDL_CreateRGBSurface(0, 1, 1, 32, 0, 0, 0, 0);
    SDL_FillRect(whiteSurface, NULL, SDL_MapRGB(whiteSurface->format, 255, 255, 255));
    SDL_FillRect(blackSurface, NULL, SDL_MapRGB(blackSurface->format, 0  , 0  , 0  ));
    SDL_FillRect(greySurface , NULL, SDL_MapRGB(greySurface->format,  127, 127, 127));    
}
void deinit()
{
    freeBoard(&gameBoard);
    freeBoard(&tempBoard);
    SDL_FreeSurface(windowSurface);
    SDL_FreeSurface(whiteSurface);
    SDL_FreeSurface(blackSurface);
    SDL_FreeSurface(greySurface);
    SDL_DestroyWindow(window);
}

int* allocateBoard(int w, int h)
{
    return (int*) calloc(sizeof(int), w * h); /* Everything starts as 0 */
}
void freeBoard(int **board)
{
    free(*board);
}
void initRects()
{
    int i, j;
    SDL_Rect *r;
    gameBoardRects = malloc(sizeof(SDL_Rect) * ROWS * COLS);
    for (i = 0; i < COLS; ++i)
    {
        for (j = 0; j < ROWS; ++j)
        {
            r =  &gameBoardRects[i + (ROWS * j)];
            r->w = CELL_W;
            r->h = CELL_H;
            r->x = i * CELL_W;
            r->y = j * CELL_H;
        }
    }
}

/* x, y is the pixel positions - store the rect position in MouseRect */
/* TODO: actually make mouse clicks change stuff, and then call draw() */
void updateMouseRect(int x, int y)
{
/*
    int i, j;
    SDL_Rect r;
    for (i = 0; i < COLS; ++i)
    {
        for (j = 0; j < ROWS; ++j)
        {
            r = gameBoardRects[i + (ROWS * j)];
            if (r.x <= x && r.x + r.w >= x && r.y <= y && r.y + r.h >= y)
            {
                MouseRect.x = i;
                MouseRect.y = j;
                return;
            }
        }
    } 
*/
    if (CELL_W == 0 || CELL_H == 0)
    {
        printf("Problem!!\tCELL_W: %d\tCELL_H: %d\n", CELL_W, CELL_H);
        return;
    }
    MouseRect.x = x / CELL_W;
    MouseRect.y = y / CELL_H;
}

/* Because I want the board to loop around, this will use w and h as modulus operators */
int getArrValue(int *arr, int x, int y, int w, int h)
{
    x = (x + w) % w;
    y = (y + w) % h;
    return arr[x + (h * y)];
}
/* I don't think this really works */
void setArr(int **arr, int x, int y, int w, int h, int val)
{
    x = (x + w) % w;
    y = (y + h) % h;
    *arr[x + (y * h)] = val;
}

void processInputs()
{
    int i;
    
    if (getInput(L_CLICK) || getInput(R_CLICK))
    {
        gameBoard[MouseRect.x + (MouseRect.y * ROWS)] = getInput(L_CLICK);
    }
    /* getFirstInput SPACE */
    if (getFirstInput(SPACE))
    {
        paused = !paused;
        printf("Paused: %d\n", paused);
    }
    if (getInput(UP_ARROW))
    {
        speed -= 10;
        if (speed < 10)
            speed = 10;
        printf("Speed: %03.0f\n", speed);
    }
    if (getInput(DOWN_ARROW))
    {
        speed += 10;
        if (speed > 1000)
            speed = 1000;
        printf("Speed: %03.0f\n", speed);
    }
    /* ENTER should clear the board, and then set game to paused */
    if (getFirstInput(ENTER))
    {
        for (i = 0; i < ROWS * COLS; ++i)
        {
            gameBoard[i] = 0;
            tempBoard[i] = 0;
        }
        paused = 1;
        printf("Paused: 1\n");
    }
    /* The 'S' button does a single step for the game */
    /* Should be getFirstInput */
    if (getFirstInput(S) && paused)
    {
        tick();
    }
    
    draw();
}

void updateInputs(SDL_Event *e)
{
    switch(e->type)
    {
    /* MouseButtonEvent */
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (e->button.button == SDL_BUTTON_LEFT)
            currInputs[L_CLICK] = e->button.state == SDL_PRESSED;
        if (e->button.button == SDL_BUTTON_RIGHT)
            currInputs[R_CLICK] = e->button.state == SDL_PRESSED;
        break;
    case SDL_MOUSEMOTION:
        Mouse.x = e->motion.x;
        Mouse.y = e->motion.y;
        updateMouseRect(Mouse.x, Mouse.y);
        break;
    /* KeyboardEvent */
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        switch(e->key.keysym.sym)
        {
        case SDLK_UP:
            currInputs[UP_ARROW] = e->key.state == SDL_PRESSED;
            break;
        case SDLK_DOWN:
            currInputs[DOWN_ARROW] = e->key.state == SDL_PRESSED;
            break;
        case SDLK_SPACE:
            currInputs[SPACE] = e->key.state == SDL_PRESSED;
            break;
        case SDLK_RETURN:
        case SDLK_RETURN2:
            currInputs[ENTER] = e->key.state == SDL_PRESSED;
            break;
        case SDLK_s:
            currInputs[S] = e->key.state == SDL_PRESSED;
            break;
        }
        break;
    /* Quit Event */
    case SDL_QUIT:
        going = 0;
        break;
    }
}

int getInput(int key)
{
    if (key < 0 || key > NUM_INPUTS)
        return 0;
    return currInputs[key];
}
int getFirstInput(int key)
{
    if (key < 0 || key > NUM_INPUTS)
        return 0;
    return currInputs[key] && !prevInputs[key];
}
int getLastInput(int key)
{
    if (key < 0 || key > NUM_INPUTS)
        return 0;
    return !currInputs[key] && prevInputs[key];
}

int countNeighbors(int **board, int x, int y)
{
    /* Nested for - first controlls y-val, from -1 to 1, and x-val from -1 to 1 */
    /* Numebrs are relative to x, y passed to function */
    int i, j;
    int count = 0;
    for (i = x - 1; i <= x + 1; ++i)
    {
        for (j = y - 1; j <= y + 1; ++j)
        {
            /* The center cell does NOT count as its own neighbor */
            if (i == x && y == j)
                continue;
            /* else if ((*board)[((i + COLS) % COLS) + (((j + ROWS) % ROWS) * ROWS)] == 1) */
            else if (getArrValue(*board, i, j, COLS, ROWS) == 1)
            {
                ++count;
            }
        }
    }
    return count;
}
void tick()
{
    /* This is where the board changes depending on the rules */
    int i, j;
    int count;
    for (i = 0; i < ROWS * COLS; ++i)
    {
        tempBoard[i] = gameBoard[i];
    }
    for (i = 0; i < COLS; ++i)
    {
        for (j = 0; j < ROWS; ++j)
        {
            count = countNeighbors(&tempBoard, i, j);
            /* If the cell is live */
            if (getArrValue(tempBoard, i, j, COLS, ROWS) == 1)
            {
                if (count < 2)
                    gameBoard[i + (j * ROWS)] = 0;
                if (count > 3)
                    gameBoard[i + (j * ROWS)] = 0;                
            }
            /* Now, if the cell is dead and has exactly 3 neighbors */
            else if (count == 3)
            {
                gameBoard[i + (j * ROWS)] = 1;
            }
        }
    }
}



/* Blit grey background, then live/dead cells (grey is the border behind cells) */
void draw()
{
    int i, j;
    SDL_Rect r;
    SDL_GetWindowSize(window, &WINDOW_W, &WINDOW_H);
    SDL_BlitScaled(greySurface, NULL, windowSurface, NULL);
    for (i = 0; i < COLS; ++i)
    {
        for (j = 0; j < ROWS; ++j)
        {
            /* Live is black, dead is white */
            r = gameBoardRects[i + (j * ROWS)];
            r.x += (BORDER / 2);
            r.y += (BORDER / 2);
            r.w -= (BORDER / 2);
            r.h -= (BORDER / 2);
            SDL_BlitScaled((getArrValue(gameBoard, i, j, COLS, ROWS) == 0 ? whiteSurface : blackSurface),
                           NULL,
                           windowSurface,
                           &r);
        }
    }
    
    SDL_UpdateWindowSurface(window);
}