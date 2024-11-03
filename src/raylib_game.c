/*******************************************************************************************
*
*   raylib gamejam template
*
*   Template originally created with raylib 4.5-dev, last time updated with raylib 5.0
*
*   Template licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2022-2024 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#if defined(PLATFORM_WEB)
    #define CUSTOM_MODAL_DIALOGS            // Force custom modal dialogs usage
    #include <emscripten/emscripten.h>      // Emscripten library - LLVM to JavaScript compiler
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

#define BOARD_WIDTH 6
#define BOARD_HEIGHT 13

#define PAL_BLACK BLACK
#define PAL_WHITE WHITE
#define PAL_GRAY GRAY

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum {
    SCREEN_LOGO = 0, 
    SCREEN_TITLE, 
    SCREEN_GAMEPLAY, 
    SCREEN_ENDING
} game_screen;

typedef enum {
    PIECE_EMPTY = 0,
    PIECE_HCONN = 1,
    PIECE_VCONN = 2,
    PIECE_UL = 3,
    PIECE_DL = 4,
    PIECE_DR = 5,
    PIECE_UR = 6,
    PIECE_DST = 7,
    PIECE_JUNK = 8,
    PIECE_FIRE = 9,
} piece;

#define PIECE_PALLETE_SIZE 10

typedef enum {
    RIGHT = 0,
    DOWN = 1,
    LEFT = 2,
    UP = 3,
} orientation;

typedef struct {
    piece Pieces[BOARD_HEIGHT][BOARD_WIDTH];
} board;

typedef struct {
    unsigned int Incoming[BOARD_HEIGHT][BOARD_WIDTH];
} power_board;

typedef struct {
    int x, y;
    piece Pieces[2];
    orientation Orientation;
} brick;

typedef struct {
    int Ys[BOARD_HEIGHT*BOARD_WIDTH];
    int Xs[BOARD_HEIGHT*BOARD_WIDTH];
    int Count;
    int OpenConns;
    bool Junk;
} trace;

typedef struct {
    float Start;
    float Duration;
} timer;

typedef struct {
    int Score;
    int NodeChain;
    int WireChain;
    int Multiplier;
} scoring;

typedef struct {
    power_board Powers;
    board Board;
    brick Brick;
    trace Trace;
    trace TraceJunk;
    timer TimerGravity;
    timer TimerTrace;
    int   TraceIndex;
    timer TimerJunk;
    int   TraceJunkIndex;
    scoring Scoring;
} gameplay;

typedef struct {
    gameplay Gameplay;
} game;

// TODO: Define your custom data types here

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static const int ScreenWidth = 224*3;
static const int ScreenHeight = 256*3;
static game Game;

// TODO: Define global variables here, recommended to make them static

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void);      // Update and Draw one frame
static void GFX_DrawBoard(power_board *Powers, board *board);
void GP_Update(gameplay *Gameplay);
void GFX_DrawBoardAndBricks(power_board *Powers, board *Board, brick *Brick);
brick Brick_Random(void);
void GFX_DrawPiece(piece Piece, int x, int y);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
#if !defined(_DEBUG)
    SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messages
#endif

    Game.Gameplay.Brick = Brick_Random();
    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(ScreenWidth, ScreenHeight, "Nettis");

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60);     // Set our game frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button
    {
        UpdateDrawFrame();
    }
#endif
    // TODO: Unload all loaded resources at this point

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------------
// Module functions definition
//--------------------------------------------------------------------------------------------
// Update and draw frame
void UpdateDrawFrame(void)
{
    GP_Update(&Game.Gameplay);

    Camera2D Camera = { 0 };
    Camera.zoom = 3.0f;
    Camera.offset = (Vector2){ 100, 100 };

    // Draw the texture to the screen
    BeginDrawing();
    {
        ClearBackground(BLACK);
        Camera.offset.x = 100;
        BeginMode2D(Camera);
        {
            GFX_DrawBoardAndBricks(&Game.Gameplay.Powers, &Game.Gameplay.Board, &Game.Gameplay.Brick);
        }
        EndMode2D();
        Camera.offset.x = 120;
        BeginMode2D(Camera);
        {
            DrawText(TextFormat("Score: %i", Game.Gameplay.Scoring.Score), 90, 10, 10, WHITE);
            GFX_DrawPiece(PIECE_DST, 6, 4);
            DrawText(TextFormat("Nodes\n\n"), 110, 62, 10, DARKGRAY); 
            GFX_DrawPiece(PIECE_HCONN, 6, 6);
            DrawText(TextFormat("Connections\n\n"), 110, 62+30, 10, DARKGRAY); 
            GFX_DrawPiece(PIECE_FIRE, 6, 8);
            DrawText(TextFormat("Fire\n\n"), 110, 62+30+30, 10, DARKGRAY); 
            GFX_DrawPiece(PIECE_JUNK, 6, 10);
            DrawText(TextFormat("Junk\n\n"), 110, 62+30+30+30, 10, DARKGRAY);
        }
        EndMode2D();
    }
    EndDrawing();
}

timer Timer_Make(float Duration)
{
    timer NewTimer;
    NewTimer.Start = GetTime();
    NewTimer.Duration = Duration;
    return NewTimer;
}

bool Timer_IsExpired(timer *Timer)
{
    return GetTime() > Timer->Start + Timer->Duration;  
}

bool Trace_Contains(trace *Trace, int x, int y)
{
    for (int i = 0; i < Trace->Count; i++)
    {
        if (Trace->Xs[i] == x && Trace->Ys[i] == y)
        {
            return true;
        }
    }

    return false;
}

orientation Orientation_Flip(orientation Orientation)
{
    switch (Orientation)
    {
        case RIGHT: return LEFT;
        case LEFT:  return RIGHT;
        case DOWN:  return UP;
        case UP:    return DOWN;
    }
}

unsigned int Piece_IncomingOrientations(piece Piece)
{
    switch (Piece)
    {
        case PIECE_EMPTY: return 0;
        case PIECE_HCONN: return 1<<RIGHT | 1<<LEFT;
        case PIECE_VCONN: return 1<<DOWN  | 1<<UP;
        case PIECE_UL:    return 1<<DOWN  | 1<<RIGHT;
        case PIECE_DL:    return 1<<UP    | 1<<RIGHT;
        case PIECE_DR:    return 1<<UP    | 1<<LEFT;
        case PIECE_UR:    return 1<<DOWN  | 1<<LEFT;
        case PIECE_DST:   return 1<<RIGHT | 1<<LEFT | 1<<DOWN | 1<<UP;
        case PIECE_JUNK:  return 0;
        case PIECE_FIRE:  return 0;
    }
}

unsigned int Piece_OutgoingOrientations(piece Piece)
{
    switch (Piece)
    {
        case PIECE_EMPTY: return 0;
        case PIECE_HCONN: return 1<<RIGHT | 1<<LEFT;
        case PIECE_VCONN: return 1<<DOWN | 1<<UP;
        case PIECE_UL:    return 1<<UP    | 1<<LEFT;
        case PIECE_DL:    return 1<<DOWN  | 1<<LEFT;
        case PIECE_DR:    return 1<<DOWN  | 1<<RIGHT;
        case PIECE_UR:    return 1<<UP    | 1<<RIGHT;
        case PIECE_DST:   return 1<<RIGHT | 1<<LEFT | 1<<DOWN | 1<<UP;
        case PIECE_JUNK:  return 0;
        case PIECE_FIRE:  return 1<<RIGHT | 1<<LEFT | 1<<DOWN | 1<<UP;
    }
}

piece Piece_Rotate(piece Piece)
{
    switch (Piece)
    {
        case PIECE_EMPTY: return PIECE_EMPTY;
        case PIECE_HCONN: return PIECE_VCONN;
        case PIECE_VCONN: return PIECE_HCONN;
        case PIECE_UL:    return PIECE_UR;
        case PIECE_DL:    return PIECE_UL;
        case PIECE_DR:    return PIECE_DL;
        case PIECE_UR:    return PIECE_DR;
        case PIECE_DST:   return PIECE_DST;
        case PIECE_JUNK:  return PIECE_JUNK;
        case PIECE_FIRE:  return PIECE_FIRE;
    }
}

bool Piece_IsConnectionType(piece Piece)
{
    switch (Piece)
    {
        case PIECE_HCONN:
        case PIECE_VCONN:
        case PIECE_UL:
        case PIECE_DL:
        case PIECE_DR:
        case PIECE_UR:
            return true;
        default:
            return false;
    }
}

brick Brick_Rotate(brick *Brick)
{
    brick NewBrick;
    memcpy(&NewBrick, Brick, sizeof(brick));

    switch (NewBrick.Orientation)
    {
        case RIGHT:
            NewBrick.Orientation = DOWN;
            break;
        case DOWN:
            NewBrick.Orientation = LEFT;
            break;
        case LEFT:
            NewBrick.Orientation = UP;
            break;
        case UP:
            NewBrick.Orientation = RIGHT;
            break;
        default:
            break;
    }

    NewBrick.Pieces[0] = Piece_Rotate(NewBrick.Pieces[0]);
    NewBrick.Pieces[1] = Piece_Rotate(NewBrick.Pieces[1]);

    return NewBrick;
}

brick Brick_Move(brick *Brick, int dx, int dy)
{
    brick NewBrick;
    memcpy(&NewBrick, Brick, sizeof(brick));

    NewBrick.x += dx;
    NewBrick.y += dy;

    return NewBrick;
}

brick Brick_Random(void)
{
    srand(time(NULL));

    const piece CHANCE_TBL[] = {
        PIECE_HCONN,
        PIECE_HCONN,
        PIECE_HCONN,
        PIECE_VCONN,
        PIECE_VCONN,
        PIECE_VCONN,
        PIECE_UL,
        PIECE_DL,
        PIECE_DR,
        PIECE_UR,
        PIECE_DST,
        PIECE_DST,
        PIECE_JUNK,
        PIECE_FIRE
    };

    const int CHANCE_TBL_SIZE = sizeof(CHANCE_TBL)/sizeof(CHANCE_TBL[0]);

    brick NewBrick;
    NewBrick.x = BOARD_WIDTH/2-1;
    NewBrick.y = 0;
    NewBrick.Orientation = rand() % 2;

    enum brick_type {
        TYPE_CONNECTION,
        TYPE_JUNK,
        TYPE_RANDOM,
        TYPE_DEST,
        TYPE_FIRE,
    };

    const enum brick_type TYPE_CHANCE_TBL[] = {
        TYPE_CONNECTION,
        TYPE_CONNECTION,
        TYPE_CONNECTION,
        TYPE_JUNK,
        TYPE_RANDOM,
        TYPE_RANDOM,
        TYPE_DEST,
        TYPE_FIRE,
    };

    enum brick_type Type = TYPE_CHANCE_TBL[rand() % (sizeof(TYPE_CHANCE_TBL)/sizeof(TYPE_CHANCE_TBL[0]))];
    printf("Type: %d\n", Type);

    if (Type == TYPE_FIRE)
    {
        NewBrick.Pieces[0] = PIECE_FIRE;
        NewBrick.Pieces[1] = PIECE_EMPTY;
        return NewBrick;
    }

    while (true) {
        NewBrick.Pieces[0] = CHANCE_TBL[rand() % CHANCE_TBL_SIZE];
        NewBrick.Pieces[1] = CHANCE_TBL[rand() % CHANCE_TBL_SIZE];

        unsigned int DirFrom = Piece_OutgoingOrientations(NewBrick.Pieces[0]);
        unsigned int DirTo = Piece_IncomingOrientations(NewBrick.Pieces[1]);

        if (NewBrick.Pieces[0] == PIECE_FIRE || NewBrick.Pieces[1] == PIECE_FIRE)
            continue;

        switch (Type) {
            case TYPE_CONNECTION:
                if (!(Piece_IsConnectionType(NewBrick.Pieces[0]) || Piece_IsConnectionType(NewBrick.Pieces[1])))
                    break;
                if ((DirFrom & DirTo & 1<<NewBrick.Orientation) != 0)
                    return NewBrick;
                break;
            case TYPE_JUNK:
                if (NewBrick.Pieces[0] != PIECE_JUNK && NewBrick.Pieces[1] != PIECE_JUNK)
                    break;
                return NewBrick;
                break;
            case TYPE_RANDOM:
                if (NewBrick.Pieces[0] == PIECE_JUNK || NewBrick.Pieces[1] == PIECE_JUNK)
                    break;
                return NewBrick;
                break;
            case TYPE_DEST:
                printf("NewBrick.Pieces[0..1]: %d, %d\n", NewBrick.Pieces[0], NewBrick.Pieces[1]);
                if (NewBrick.Pieces[0] == PIECE_JUNK || NewBrick.Pieces[1] == PIECE_JUNK)
                    break;
                if (NewBrick.Pieces[0] != PIECE_DST && NewBrick.Pieces[1] != PIECE_DST)
                    break;
                if (NewBrick.Pieces[0] == PIECE_DST && NewBrick.Pieces[1] == PIECE_DST)
                    break;
                return NewBrick;
                break;
            default:
                break;
        }
    }

    return NewBrick;
}

void Brick_Locations(brick *Brick, int x[2], int y[2])
{
    switch (Brick->Orientation)
    {
        case RIGHT:
            x[0] = Brick->x;
            x[1] = Brick->x+1;
            y[0] = Brick->y;
            y[1] = Brick->y;
            break;
        case DOWN:
            x[0] = Brick->x;
            x[1] = Brick->x;
            y[0] = Brick->y;
            y[1] = Brick->y+1;
            break;
        case LEFT:
            x[0] = Brick->x;
            x[1] = Brick->x-1;
            y[0] = Brick->y;
            y[1] = Brick->y;
            break;
        case UP:
            x[0] = Brick->x;
            x[1] = Brick->x;
            y[0] = Brick->y;
            y[1] = Brick->y-1;
            break;
    }
}

void Board_PutTileSafe(board *Board, int x, int y, piece Piece)
{
    if (Piece == PIECE_EMPTY)
    {
        return;
    }

    if (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT)
    {
        Board->Pieces[y][x] = Piece;
    }
}

void Board_PutBrick(board *Board, brick *Brick)
{
    int x[2], y[2];
    Brick_Locations(Brick, x, y);
    Board_PutTileSafe(Board, x[0], y[0], Brick->Pieces[0]);
    Board_PutTileSafe(Board, x[1], y[1], Brick->Pieces[1]);
}

brick Board_BumpBrick(board *Board, brick *Brick)
{
    brick NewBrick;
    memcpy(&NewBrick, Brick, sizeof(brick));
    if (NewBrick.Pieces[0] == PIECE_EMPTY || NewBrick.Pieces[1] == PIECE_EMPTY)
    {
        while (true) {
            int x = NewBrick.x, y = NewBrick.y;
            if (x < 0) NewBrick.x++;
            else if (x >= BOARD_WIDTH) NewBrick.x--;
            else if (y < 0) NewBrick.y++;
            else break;
        }
        return NewBrick;
    }

    int x[2], y[2];
    while (true) {
        Brick_Locations(&NewBrick, x, y);

        if (x[0] < 0 || x[1] < 0) NewBrick.x++;
        else if (x[0] >= BOARD_WIDTH || x[1] >= BOARD_WIDTH) NewBrick.x--;
        else if (y[0] < 0 || y[1] < 0) NewBrick.y++;
        else break;
    }

    return NewBrick;
}

bool Board_IsOob(board *Board, int x, int y)
{
    return x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT;
}

bool Board_IsOccupied(board *Board, int x, int y)
{
    if (Board_IsOob(Board, x, y)) 
        return true;

    return Board->Pieces[y][x] != PIECE_EMPTY;
}

bool Board_ShouldPlaceBrick(board *Board, brick *Brick)
{
    int x[2], y[2];
    Brick_Locations(Brick, x, y);

    for (int i = 0; i < 2; i++)
    {
        if (Brick->Pieces[i] == PIECE_EMPTY)
        {
            continue;
        }

        if (Board_IsOccupied(Board, x[i], y[i]))
        {
            return true;
        }
    }
    
    return false;
}

bool Board_GravityStep(board *Board)
{
    bool HasMoved = false;

    for (int y = 0; y < BOARD_HEIGHT-1; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            if (Board->Pieces[y][x] == PIECE_EMPTY || Board->Pieces[y+1][x] != PIECE_EMPTY)
            {
                continue;
            }

            piece Piece = Board->Pieces[y][x];
            Board->Pieces[y][x] = PIECE_EMPTY;
            Board->Pieces[y+1][x] = Piece;
            HasMoved = true;
        }
    }

    return HasMoved;
}

trace Board_TraceFilter(board *Board, trace *Trace, power_board *Powers)
{
    trace NewTrace = { 0 };
    memcpy(&NewTrace, Trace, sizeof(trace));

    // int Count = 0;
    // for (int i = 0; i < Trace->Count; i++)
    // {
    //     if (Board->Pieces[Trace->Ys[i]][Trace->Xs[i]] == PIECE_DST)
    //     {
    //         Count++;
    //         if (Count > 1) {
    //             return NewTrace;
    //         }
    //     }
    // }

    NewTrace.Count = 0;

    for (int i = 0; i < Trace->Count; i++)
    {
        int x = Trace->Xs[i];
        int y = Trace->Ys[i];
        piece Piece = Board->Pieces[y][x];

        if (!Piece_IsConnectionType(Piece))
        {
            NewTrace.Xs[NewTrace.Count] = x;
            NewTrace.Ys[NewTrace.Count] = y;
            NewTrace.Count++;
            continue;
        }

        unsigned int DirFrom = Piece_OutgoingOrientations(Piece);
        if ((Powers->Incoming[y][x] & DirFrom) != DirFrom)
        {
            continue;
        }

        NewTrace.Xs[NewTrace.Count] = x;
        NewTrace.Ys[NewTrace.Count] = y;
        NewTrace.Count++;
    }

    return NewTrace;
}

bool Board_DoTraceIter(board *Board, trace *Trace, power_board *Powers)
{
    trace NewTrace;
    memcpy(&NewTrace, Trace, sizeof(trace));
    bool HasIter = false;

    for (int i = 0; i < Trace->Count; i++)
    {
        int x = Trace->Xs[i];
        int y = Trace->Ys[i];
        piece Piece = Board->Pieces[y][x];
        unsigned int DirFrom = Piece_OutgoingOrientations(Piece);

        struct {
            int x, y;
            orientation Orientation;
        } Positions[4] = {
            { x-1, y, LEFT },
            { x+1, y, RIGHT },
            { x, y-1, UP },
            { x, y+1, DOWN },
        };

        for (int i = 0; i < 4; i++) {
            int x = Positions[i].x;
            int y = Positions[i].y;

            if (Board_IsOob(Board, x, y) || Trace_Contains(Trace, x, y))
                continue;
            
            piece OtherPiece = Board->Pieces[y][x];

            if (OtherPiece == PIECE_DST && Piece == PIECE_DST)
            {
                continue;
            }

            unsigned int DirTo = Piece_IncomingOrientations(OtherPiece);

            if ((DirFrom & DirTo & (1<<Positions[i].Orientation)) == 0)
            {
                continue;
            }

            Powers->Incoming[y][x] |= 1<<Orientation_Flip(Positions[i].Orientation);

            NewTrace.Xs[NewTrace.Count] = x;
            NewTrace.Ys[NewTrace.Count] = y;
            NewTrace.Count++;
            HasIter = true;
        }
    }

    memcpy(Trace, &NewTrace, sizeof(trace));
    return HasIter;
}

trace Board_Trace(board *Board, int x, int y, power_board *Powers)
{
    trace Trace = { 0 };
    Trace.Count = 1;
    Trace.Xs[0] = x;
    Trace.Ys[0] = y;
    while (Board_DoTraceIter(Board, &Trace, Powers))
        ;
    return Trace;
}

bool Board_DoTraceIterJunk(board *Board, trace *Trace)
{
    trace NewTrace;
    memcpy(&NewTrace, Trace, sizeof(trace));
    bool HasIter = false;

    for (int i = 0; i < Trace->Count; i++)
    {
        int x = Trace->Xs[i];
        int y = Trace->Ys[i];
        piece Piece = Board->Pieces[y][x];
        unsigned int DirFrom = Piece_OutgoingOrientations(Piece);

        struct {
            int x, y;
            orientation Orientation;
        } Positions[4] = {
            { x-1, y, LEFT },
            { x+1, y, RIGHT },
            { x, y-1, UP },
            { x, y+1, DOWN },
        };

        for (int i = 0; i < 4; i++) {
            int x = Positions[i].x;
            int y = Positions[i].y;

            if ((DirFrom & (1<<Positions[i].Orientation)) == 0)
            {
                continue;
            }

            if (Trace_Contains(Trace, x, y))
            {
                continue;
            }
 
            if (Board_IsOob(Board, x, y))
            {
                NewTrace.Junk = true;
                continue;
            }
            
            piece OtherPiece = Board->Pieces[y][x];

            if (OtherPiece == PIECE_DST)
            {
                continue;
            }

            // if (OtherPiece == PIECE_JUNK)
            // {
            //     NewTrace.Junk = true;
            // }

            unsigned int DirTo = Piece_IncomingOrientations(OtherPiece);

            if ((DirFrom & DirTo & (1<<Positions[i].Orientation)) == 0)
            {
                if (OtherPiece == PIECE_EMPTY)
                {
                    NewTrace.OpenConns++;
                }
                continue;
            }

            NewTrace.OpenConns++;
            NewTrace.Xs[NewTrace.Count] = x;
            NewTrace.Ys[NewTrace.Count] = y;
            NewTrace.Count++;
            HasIter = true;
        }
    }

    memcpy(Trace, &NewTrace, sizeof(trace));
    return HasIter;
}

bool Board_DoTraceIterFire(board *Board, trace *Trace)
{
    trace NewTrace;
    memcpy(&NewTrace, Trace, sizeof(trace));
    bool HasIter = false;

    for (int i = 0; i < Trace->Count; i++)
    {
        int x = Trace->Xs[i];
        int y = Trace->Ys[i];
        piece Piece = Board->Pieces[y][x];
        unsigned int DirFrom = Piece_OutgoingOrientations(Piece);

        struct {
            int x, y;
            orientation Orientation;
        } Positions[4] = {
            { x-1, y, LEFT },
            { x+1, y, RIGHT },
            { x, y-1, UP },
            { x, y+1, DOWN },
        };

        if (Piece != PIECE_FIRE && !Piece_IsConnectionType(Piece))
        {
            continue;
        }

        for (int i = 0; i < 4; i++) {
            int x = Positions[i].x;
            int y = Positions[i].y;

            if (Board_IsOob(Board, x, y) || Trace_Contains(Trace, x, y))
            {
                continue;
            }
            
            piece OtherPiece = Board->Pieces[y][x];

            if (!Piece_IsConnectionType(OtherPiece))
            {
                continue;
            }

            unsigned int DirTo = Piece_IncomingOrientations(OtherPiece);

            if ((DirFrom & DirTo & (1<<Positions[i].Orientation)) == 0)
            {
                continue;
            }

            NewTrace.Xs[NewTrace.Count] = x;
            NewTrace.Ys[NewTrace.Count] = y;
            NewTrace.Count++;
            HasIter = true;
        }
    }

    memcpy(Trace, &NewTrace, sizeof(trace));
    return HasIter;
}

trace Board_TraceJunk(board *Board, int x, int y)
{
    trace Trace = { 0 };
    Trace.Count = 1;
    Trace.Xs[0] = x;
    Trace.Ys[0] = y;
    while (Board_DoTraceIterJunk(Board, &Trace))
        ;
    return Trace;
}

trace Board_TraceFire(board *Board, int x, int y)
{
    trace Trace = { 0 };
    Trace.Count = 1;
    Trace.Xs[0] = x;
    Trace.Ys[0] = y;
    while (Board_DoTraceIterFire(Board, &Trace))
        ;
    return Trace;
}

trace Board_GetTrace(board *Board, power_board *Powers)
{
    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            if (Board->Pieces[y][x] == PIECE_FIRE)
            {
                trace Trace = Board_TraceFire(Board, x, y);
                if (Trace.Count > 1) return Trace;
            }
            if (Board->Pieces[y][x] == PIECE_DST)
            {
                Board_Trace(Board, x, y, Powers);
            }
        }
    }

    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            if (Board->Pieces[y][x] == PIECE_DST)
            {
                trace Trace = Board_Trace(Board, x, y, Powers);
                Trace = Board_TraceFilter(Board, &Trace, Powers);
                if (Trace.Count > 1) return Trace; 
            }
        }
    }

    return (trace){ 0 };
}

trace Board_GetTraceJunk(board *Board)
{
    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            if (Piece_IsConnectionType(Board->Pieces[y][x]))
            {
                trace Trace = Board_TraceJunk(Board, x, y);
                if (Trace.Junk) return Trace;
                // if (Trace.OpenConns < 2) return Trace;
            }
        }
    }

    return (trace){ 0 };
}

void Board_CleanSurroundings(board *Board, int x, int y)
{
    struct {
        int x, y;
    } Positions[8] = {
        { 0, 1 },
        { 1, 0 },
        { 0, -1 },
        { -1, 0 },
        { 1, 1 },
        { -1, 1 },
        { 1, -1 },
        { -1, -1 },
    };

    for (int i = 0; i < 8; i++)
    {
        int x2 = x + Positions[i].x;
        int y2 = y + Positions[i].y;

        if (Board_IsOob(Board, x2, y2))
        {
            continue;
        }

        if (Board->Pieces[y2][x2] == PIECE_JUNK)
        {
            Board->Pieces[y2][x2] = PIECE_EMPTY;
        }
    }
}

void GP_Update(gameplay *Gameplay)
{
    Gameplay->Powers = (power_board){ 0 };
    if (Gameplay->TraceIndex >= Gameplay->Trace.Count)
    {
        Gameplay->Scoring.Multiplier += 1;
        Gameplay->TraceIndex = 0;
        Gameplay->Trace = Board_GetTrace(&Gameplay->Board, &Gameplay->Powers);
    }
    else
    {
        if (Timer_IsExpired(&Gameplay->TimerTrace))
        {
            int x = Gameplay->Trace.Xs[Gameplay->TraceIndex];
            int y = Gameplay->Trace.Ys[Gameplay->TraceIndex];
            if (Gameplay->Board.Pieces[y][x] == PIECE_DST)
            {
                Gameplay->Scoring.NodeChain += 1;
            }
            Gameplay->Scoring.WireChain += 1;
            Gameplay->Board.Pieces[y][x] = PIECE_EMPTY;
            Gameplay->Scoring.Score += 10*Gameplay->Scoring.Multiplier*(Gameplay->Scoring.NodeChain+1)*(Gameplay->Scoring.WireChain+1);
            Board_CleanSurroundings(&Gameplay->Board, x, y);
            Gameplay->TimerTrace = Timer_Make(0.15f);
            Gameplay->TraceIndex = (Gameplay->TraceIndex + 1);
        }
        return;
    }

    if (Gameplay->TraceJunkIndex >= Gameplay->TraceJunk.Count)
    {
        Gameplay->Scoring.Multiplier += 1;
        Gameplay->TraceJunkIndex = 0;
        Gameplay->TraceJunk = Board_GetTraceJunk(&Gameplay->Board);
    }
    else
    {
        if (Timer_IsExpired(&Gameplay->TimerJunk))
        {
            int x = Gameplay->TraceJunk.Xs[Gameplay->TraceJunkIndex];
            int y = Gameplay->TraceJunk.Ys[Gameplay->TraceJunkIndex];
            Gameplay->Board.Pieces[y][x] = PIECE_JUNK;
            Gameplay->TimerJunk = Timer_Make(0.15f);
            Gameplay->TraceJunkIndex = (Gameplay->TraceJunkIndex + 1);
        }
        return;
    }

    if (Gameplay->TraceJunk.Count != 0 || Gameplay->Trace.Count != 0)
    {
        return;
    }

    Gameplay->Scoring.NodeChain = 0;
    Gameplay->Scoring.WireChain = 0;
    Gameplay->Scoring.Multiplier = 0;

    brick NewBrick = Gameplay->Brick;
    int dx = 0, dy = 0;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN))
    {
        dy = 1;
    }
    else if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT))
    {
        dx = -1;
    }
    else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT))
    {
        dx = 1;
    }
    else if (IsKeyPressed(KEY_Z) || IsKeyPressedRepeat(KEY_Z))
    {
        NewBrick = Brick_Rotate(&Gameplay->Brick);
        if (Board_ShouldPlaceBrick(&Gameplay->Board, &NewBrick)) {
            NewBrick = Gameplay->Brick;
        }

        Gameplay->Brick = NewBrick;
    }
    
    if (Timer_IsExpired(&Gameplay->TimerGravity))
    {
        Gameplay->TimerGravity = Timer_Make(0.75f);
        dy += 1;
    }

    if (dx != 0 || dy != 0)
    {
        NewBrick = Brick_Move(&Gameplay->Brick, dx, dy);
        if (Board_ShouldPlaceBrick(&Gameplay->Board, &NewBrick))
        {
            if (dy != 0)
            {
                Board_PutBrick(&Gameplay->Board, &Gameplay->Brick);
                Gameplay->Brick = Brick_Random();
                if (Board_ShouldPlaceBrick(&Gameplay->Board, &Gameplay->Brick))
                {
                    Gameplay->Scoring = (scoring){ 0 };
                    Gameplay->Board = (board){ 0 };
                }
            }
        }
        else
        {
            Gameplay->Brick = NewBrick;
        }
    }

    while (Board_GravityStep(&Gameplay->Board))
        ;
}

const int CELL_SIZE = 16;

void GFX_DrawCellLines(unsigned int Parts, Color color, int x, int y, int Thickness)
{   
    const int sz = CELL_SIZE-1;
    const float offs = Thickness / 2.0; 

    if (Parts & 1<<RIGHT) DrawLineEx((Vector2){x*sz+CELL_SIZE/2-offs, y*sz+CELL_SIZE/2}, (Vector2){x*sz+CELL_SIZE, y*sz+CELL_SIZE/2}, Thickness, color);
    if (Parts & 1<<LEFT)  DrawLineEx((Vector2){x*sz, y*sz+CELL_SIZE/2}, (Vector2){x*sz+CELL_SIZE/2+offs, y*sz+CELL_SIZE/2}, Thickness, color);
    if (Parts & 1<<DOWN)  DrawLineEx((Vector2){x*sz+CELL_SIZE/2, y*sz+CELL_SIZE/2-offs}, (Vector2){x*sz+CELL_SIZE/2, y*sz+CELL_SIZE}, Thickness, color);
    if (Parts & 1<<UP)    DrawLineEx((Vector2){x*sz+CELL_SIZE/2, y*sz}, (Vector2){x*sz+CELL_SIZE/2, y*sz+CELL_SIZE/2+offs}, Thickness, color);
}

void GFX_DrawPiece(piece Piece, int x, int y)
{
    const Color PIECE_PALLETE[PIECE_PALLETE_SIZE] = {
        PAL_BLACK,
        PAL_WHITE,
        PAL_WHITE,
        PAL_WHITE,
        PAL_WHITE,
        PAL_WHITE,
        PAL_WHITE,
        BLUE,
        DARKGRAY,
        ORANGE
    };

    int parts = Piece_OutgoingOrientations(Piece);
    const int sz = CELL_SIZE-1;

    if (Piece_IsConnectionType(Piece)) {
        GFX_DrawCellLines(parts, PIECE_PALLETE[Piece], x, y, 1);
    }

    switch (Piece)
    {
        case PIECE_DST:
        case PIECE_JUNK:
        case PIECE_FIRE:
            DrawRectangle(x*sz, y*sz, CELL_SIZE-1, CELL_SIZE-1, PIECE_PALLETE[Piece]);
            break;
        default:
            break;
    }

    switch (Piece)
    {
        case PIECE_DST:
            DrawRectangle(x*sz+1, y*sz+1, CELL_SIZE-3, CELL_SIZE-3, BLACK);  
            DrawRectangle(x*sz+2, y*sz+2, CELL_SIZE-5, CELL_SIZE-5, BLUE);  
            break;
        case PIECE_JUNK:
            DrawRectangle(x*sz+1, y*sz+1, CELL_SIZE-3, CELL_SIZE-3, DARKGRAY);  
            break;
        case PIECE_FIRE:
            DrawRectangle(x*sz+1, y*sz+1, CELL_SIZE-3, CELL_SIZE-3, BLACK);  
            DrawRectangle(x*sz+2, y*sz+2, CELL_SIZE-5, CELL_SIZE-5, WHITE);  
        default:
            break;
    }
}

void GFX_DrawBoardAndBricks(power_board *Powers, board *Board, brick *Brick)
{
    board VirtualBoard;
    memcpy(&VirtualBoard, Board, sizeof(board));
    Board_PutBrick(&VirtualBoard, Brick);
    GFX_DrawBoard(Powers, &VirtualBoard);
}

void GFX_DrawBoard(power_board *Powers, board *Board)
{
    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            if (!Piece_IsConnectionType(Board->Pieces[y][x]))
            {
                continue;
            }

            if (Powers->Incoming[y][x] == 0)
            {
                continue;
            }

            unsigned int Dir = Piece_OutgoingOrientations(Board->Pieces[y][x]);

            GFX_DrawCellLines(Dir, BLUE, x, y, 3);
        }
    }


    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            DrawRectangleLines(x*(CELL_SIZE-1), y*(CELL_SIZE-1), CELL_SIZE-1, CELL_SIZE-1, DARKGRAY);  
        }
    }

    DrawRectangleLinesEx((Rectangle){-1, -1, (CELL_SIZE-1)*BOARD_WIDTH+2, (CELL_SIZE-1)*BOARD_HEIGHT+2}, 1, GRAY);

    for (int y = 0; y < BOARD_HEIGHT; y++)
    {
        for (int x = 0; x < BOARD_WIDTH; x++)
        {
            GFX_DrawPiece(Board->Pieces[y][x], x, y);
        }
    }
}