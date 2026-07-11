/*
    ============================================================
     2D Maze Explorer
     Programming Fundamentals Project

     Students : Maaz Hussain (BSCS25101037)
                Muhammad Ahzam (BSCS25101017)
     Instructor: Nida E Rub

     Description:
     A console based maze game written entirely in C. A brand
     new maze is randomly generated every time the program runs
     (and every time a new level starts). The player types in
     their name, then moves through the maze using the W, A, S,
     D keys and tries to reach the exit. Reaching the exit scores
     points and advances the player to the next level, where a
     new, randomly generated maze appears. Clearing 3 levels in
     a row wins the game.

     ------------------------------------------------------------

       main()
         -> showWelcomeScreen()   : title screen, wait for Enter
         -> getPlayerName()       : ask for the player's name
         -> loop on showMainMenu():
              1 -> playGame()       (the actual game)
              2 -> showInstructions()
              3 -> exit the loop

       playGame() itself loops over LEVELS (1..MAX_LEVELS).
       For every level it:
         a) builds a brand new maze  -> drawBorders() + generateMaze()
         b) repeatedly: draws the maze, reads one key, moves the
            player if that direction is not a wall, checks if the
            player reached the exit
         c) once the exit is reached, adds points and goes to the
            next level (or shows "YOU WIN" on the last level)

       Maze generation (generateMaze) is just 3 simple steps:
         1) openAllRooms()    : every "room" in the grid starts open
         2) carveMainPath()   : force one guaranteed route from the
                                 start to the exit (random right/down
                                 steps), so the maze can always be won
         3) addExtraOpenings(): randomly knock down a few more walls
                                 so it looks like a real maze with
                                 branches, not one straight corridor
    ============================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

/* ---------------- board / maze settings ---------------- */
#define ROWS 21
#define COLS 39
#define ROW_MAX (ROWS - 1)
#define COL_MAX (COLS - 1)

/* The maze interior is carved on a grid of "cells" (think of
   it like a smaller grid hiding inside the big character grid).
   Each cell takes up 2 rows/columns in the real array: one
   row/column for the cell itself, and one for the wall that
   might exist between it and its neighbour. That is why the
   cell-grid is roughly half the size of the real maze. */
#define CELL_ROWS 10
#define CELL_COLS 19

/* Every cell is printed TWICE as wide as it is tall, purely so
   the maze does not look squashed sideways on screen. */
#define RENDER_WIDTH 2

#define PLAYER_CHAR '@'
#define EXIT_CHAR   '$'
#define WALL_CHAR   '#'
#define PATH_CHAR   ' '

#define MAX_NAME_LEN 30

/* the game is won after clearing this many mazes in a row */
#define MAX_LEVELS 3

/* percent chance to add a random EXTRA connection between two
   neighbouring rooms, on top of the one guaranteed path, so the
   maze has side-branches instead of being one straight corridor */
#define EXTRA_OPENING_CHANCE 30

/* ---------------- function prototypes ---------------- */
void drawBorders(char maze[ROWS][COLS]);

void fillMazeWithWalls(char maze[ROWS][COLS]);
void openAllRooms(char maze[ROWS][COLS]);
void carveMainPath(char maze[ROWS][COLS]);
void addExtraOpenings(char maze[ROWS][COLS]);
void generateMaze(char maze[ROWS][COLS]);

void displayMaze(char maze[ROWS][COLS], int playerRow, int playerCol);
int getInputChar(void);
void clearScreen(void);
void showWelcomeScreen(void);
void getPlayerName(char name[]);
int  showMainMenu(char name[], int lastScore);
void showInstructions(void);
int  calculateMazeScore(int level, int moves);

int  isWalkable(char maze[ROWS][COLS], int row, int col);
int  tryMove(char maze[ROWS][COLS], int *playerRow, int *playerCol, int key);

int playGame(char name[]);

/* ============================================================
   drawBorders
   Draws only the OUTER rectangle of the maze: a "=" roof, two
   "|" side walls, and a "=" base. The inside is filled in
   separately by generateMaze().
   ============================================================ */
void drawBorders(char maze[ROWS][COLS]){
    int col;
    int row;

    /* roof */
    for(col = 0; col <= COL_MAX; col++){
        maze[0][col] = '=';
    }

    /* left wall, right wall, and empty space in between */
    for(row = 1; row <= ROW_MAX; row++){
        for(col = 0; col <= COL_MAX; col++){
            if(col == 0 || col == COL_MAX){
                maze[row][col] = '|';
            }
            else{
                maze[row][col] = PATH_CHAR;
            }
        }
    }

    /* base */
    for(col = 0; col <= COL_MAX; col++){
        maze[ROW_MAX][col] = '=';
    }
}

/* ============================================================
   fillMazeWithWalls
   STEP 1 of maze generation: before carving any paths, the
   entire inside of the maze is solid wall ('#'). Paths are
   opened up afterwards by carveMazePaths().
   ============================================================ */
void fillMazeWithWalls(char maze[ROWS][COLS]){
    int row;
    int col;

    for(row = 1; row <= ROW_MAX - 1; row++){
        for(col = 1; col <= COL_MAX - 1; col++){
            maze[row][col] = WALL_CHAR;
        }
    }
}

/* ============================================================
   openAllRooms
   STEP 2a of maze generation: every "room" position in the
   cell-grid starts out open (walkable). The walls BETWEEN
   rooms are what actually shapes the maze - they get added
   back in selectively by carveMainPath() / addExtraOpenings().
   ============================================================ */
void openAllRooms(char maze[ROWS][COLS]){
    int r;
    int c;

    for(r = 0; r < CELL_ROWS; r++){
        for(c = 0; c < CELL_COLS; c++){
            maze[1 + 2 * r][1 + 2 * c] = PATH_CHAR;
        }
    }
}

/* ============================================================
   carveMainPath
   STEP 2b of maze generation: guarantees the maze can always
   be solved.

   Simple explanation for the viva:
     - Start at room (0,0) - the top-left room, where the
       player begins.
     - At every step, randomly choose to move DOWN or RIGHT
       (whichever is still possible) and knock down the wall
       in that direction.
     - Because we only ever move down or right, and the exit
       room is always the bottom-right room, this loop is
       guaranteed to finish, and it always finishes exactly at
       the exit room. No backtracking and no extra data
       structure needed - just two counters and a while loop.
   ============================================================ */
void carveMainPath(char maze[ROWS][COLS]){
    int cellRow = 0;
    int cellCol = 0;

    while(cellRow < CELL_ROWS - 1 || cellCol < CELL_COLS - 1){
        int canGoDown  = (cellRow < CELL_ROWS - 1);
        int canGoRight = (cellCol < CELL_COLS - 1);
        int goDown;

        if(canGoDown && canGoRight){
            goDown = rand() % 2; /* 50/50 random choice */
        }
        else if(canGoDown){
            goDown = 1; /* already at the last column, must go down */
        }
        else{
            goDown = 0; /* already at the last row, must go right */
        }

        if(goDown == 1){
            /* knock down the wall directly below the current room */
            maze[1 + 2 * cellRow + 1][1 + 2 * cellCol] = PATH_CHAR;
            cellRow++;
        }
        else{
            /* knock down the wall directly to the right of the current room */
            maze[1 + 2 * cellRow][1 + 2 * cellCol + 1] = PATH_CHAR;
            cellCol++;
        }
    }
}

/* ============================================================
   addExtraOpenings
   STEP 2c of maze generation: purely cosmetic. Goes through
   every pair of neighbouring rooms and, with a small random
   chance, knocks down that wall too. This gives the maze extra
   branches and loops so it does not look like one straight
   tunnel from start to exit. Since this can only ever OPEN
   extra walls (never close the guaranteed path from
   carveMainPath), the maze stays solvable no matter what.
   ============================================================ */
void addExtraOpenings(char maze[ROWS][COLS]){
    int r;
    int c;

    for(r = 0; r < CELL_ROWS; r++){
        for(c = 0; c < CELL_COLS; c++){
            /* chance to open the wall below this room */
            if(r < CELL_ROWS - 1){
                if(rand() % 100 < EXTRA_OPENING_CHANCE){
                    maze[1 + 2 * r + 1][1 + 2 * c] = PATH_CHAR;
                }
            }

            /* chance to open the wall to the right of this room */
            if(c < CELL_COLS - 1){
                if(rand() % 100 < EXTRA_OPENING_CHANCE){
                    maze[1 + 2 * r][1 + 2 * c + 1] = PATH_CHAR;
                }
            }
        }
    }
}

/* ============================================================
   generateMaze
   Builds one brand new random maze using 3 simple steps - open
   every room, force one guaranteed path to the exit, then
   sprinkle in a few extra random openings for variety.
   ============================================================ */
void generateMaze(char maze[ROWS][COLS]){
    fillMazeWithWalls(maze);
    openAllRooms(maze);
    carveMainPath(maze);
    addExtraOpenings(maze);
}

/* ============================================================
   displayMaze
   Prints the whole maze, one character at a time, with the
   player's current position drawn on top of the maze. Each
   character is printed RENDER_WIDTH times so the maze looks
   wider instead of squashed.
   ============================================================ */
void displayMaze(char maze[ROWS][COLS], int playerRow, int playerCol){
    for(int row = 0; row <= ROW_MAX; row++){
        for(int col = 0; col <= COL_MAX; col++){
            char cellChar;

            if(row == playerRow && col == playerCol){
                cellChar = PLAYER_CHAR;
            }
            else{
                cellChar = maze[row][col];
            }

            for(int w = 0; w < RENDER_WIDTH; w++){
                printf("%c", cellChar);
            }
        }
        printf("\n");
    }
}

/* ============================================================
   getInputChar
   Reads ONE key press immediately, without waiting for Enter.

   Simple explanation for the viva:
     - By default, C waits for the Enter key before it gives you
       any input (this is called "canonical mode").
     - On Windows, _getch() already reads a single key with no
       Enter needed, so we just use that directly.
     - On Linux/Mac, there is no ready-made function like that,
       so we temporarily turn OFF canonical mode and screen-echo
       on the terminal, read exactly one character, and then
       turn those settings back ON before returning - so the
       rest of the program behaves normally afterwards.
   ============================================================ */
int getInputChar(void){
#ifdef _WIN32
    return _getch();
#else
    struct termios oldSettings;
    struct termios newSettings;
    int input;

    if(tcgetattr(STDIN_FILENO, &oldSettings) != 0){
        /* not a real terminal (e.g. input piped from a file) -
           just fall back to normal input */
        input = getchar();
        return input;
    }

    newSettings = oldSettings;
    newSettings.c_lflag &= ~(ICANON | ECHO); /* turn off line-buffering + echo */
    tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);

    input = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings); /* restore normal mode */

    return input;
#endif
}

/* ============================================================
   clearScreen
   Clears the console between frames/levels.
   ============================================================ */
void clearScreen(void){
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

/* ============================================================
   showWelcomeScreen
   The very first screen the player sees. Waits for the Enter
   key before moving on to the name entry screen.
   ============================================================ */
void showWelcomeScreen(void){
    clearScreen();
    printf("============================================================\n");
    printf("                     2D MAZE EXPLORER\n");
    printf("============================================================\n\n");
    printf("              Press ENTER to begin...");

    int ch;
    while((ch = getchar()) != '\n' && ch != EOF){
        /* just waiting here for Enter */
    }
}

/* ============================================================
   getPlayerName
   Asks the player to type their name (normal line input, since
   a name needs more than one key press). Falls back to
   "Player" if nothing is typed.
   ============================================================ */
void getPlayerName(char name[]){
    clearScreen();
    printf("============================================================\n");
    printf("                     2D MAZE EXPLORER\n");
    printf("============================================================\n\n");
    printf("Enter your name: ");

    fgets(name, MAX_NAME_LEN, stdin);

    /* strip the trailing newline that fgets keeps */
    int len = (int) strlen(name);
    for(int i = 0; i < len; i++){
        if(name[i] == '\n'){
            name[i] = '\0';
            break;
        }
    }

    if(strlen(name) == 0){
        strcpy(name, "Player");
    }
}

/* ============================================================
   showMainMenu
   Shows the player's name and last score, then returns the
   menu option they picked as a plain number (1, 2 or 3).
   ============================================================ */
int showMainMenu(char name[], int lastScore){
    int choice;

    clearScreen();
    printf("============================================================\n");
    printf("                     2D MAZE EXPLORER\n");
    printf("============================================================\n\n");
    printf("   Player: %-20s   Score: %d\n\n", name, lastScore);
    printf("   1. Start Game\n");
    printf("   2. Instructions\n");
    printf("   3. Exit\n\n");
    printf("   Choose an option (1-3): ");

    choice = getInputChar();
    printf("\n");

    if(choice == EOF){
        return 3; /* stdin closed unexpectedly - exit cleanly instead of looping forever */
    }

    return choice - '0';
}

/* ============================================================
   showInstructions
   ============================================================ */
void showInstructions(void){
    clearScreen();
    printf("============================================================\n");
    printf("                     HOW TO PLAY\n");
    printf("============================================================\n\n");
    printf("  - A brand new maze is generated every time you play.\n");
    printf("  - Your character is shown as  %c\n", PLAYER_CHAR);
    printf("  - The exit is shown as        %c\n", EXIT_CHAR);
    printf("  - Walls are shown as  %c , %c , %c\n", WALL_CHAR, '=', '|');
    printf("  - Move using:\n");
    printf("        W = up\n");
    printf("        A = left\n");
    printf("        S = down\n");
    printf("        D = right\n");
    printf("  - Press Q at any time to quit the game.\n");
    printf("  - Reach the exit to clear the level and move to the\n");
    printf("    next, harder maze!\n");
    printf("  - Clear all %d levels in a row to WIN the game!\n\n", MAX_LEVELS);
    printf("  SCORING:\n");
    printf("  - Beating a maze earns you points for that maze alone.\n");
    printf("  - Fewer moves and higher levels earn more points.\n");
    printf("  - Your total score is shown at the top while you play,\n");
    printf("    and your final score is shown when you quit (Q).\n\n");
    printf("Press any key to return to the main menu...");
    getInputChar();
}

/* ============================================================
   calculateMazeScore
   Works out how many points a single cleared maze is worth.
   Later levels are worth more (level * 50), and finishing in
   fewer moves earns a bigger bonus (200 - moves, never below
   0). So the score can never be lower than (level * 50).
   ============================================================ */
int calculateMazeScore(int level, int moves){
    int levelBonus = level * 50;
    int efficiencyBonus = 200 - moves;

    if(efficiencyBonus < 0){
        efficiencyBonus = 0;
    }

    return levelBonus + efficiencyBonus;
}

/* ============================================================
   isWalkable
   Small helper: true (1) if the player is allowed to step onto
   this character, false (0) if it is a wall character.
   ============================================================ */
int isWalkable(char maze[ROWS][COLS], int row, int col){
    char target = maze[row][col];

    if(target == WALL_CHAR || target == '=' || target == '|'){
        return 0;
    }
    return 1;
}

/* ============================================================
   tryMove
   Given one key press, works out which direction it means and
   moves the player there IF that direction is not a wall.
   Returns 1 if the player actually moved, 0 otherwise (this
   matches "isMoveKey" + the wall-check from the original code,
   just grouped into one clearly-named helper for the viva).
   ============================================================ */
int tryMove(char maze[ROWS][COLS], int *playerRow, int *playerCol, int key){
    int newRow = *playerRow;
    int newCol = *playerCol;
    int isMoveKey = 0;

    if(key == 'w' || key == 'W'){
        newRow = *playerRow - 1;
        isMoveKey = 1;
    }
    else if(key == 's' || key == 'S'){
        newRow = *playerRow + 1;
        isMoveKey = 1;
    }
    else if(key == 'a' || key == 'A'){
        newCol = *playerCol - 1;
        isMoveKey = 1;
    }
    else if(key == 'd' || key == 'D'){
        newCol = *playerCol + 1;
        isMoveKey = 1;
    }

    if(isMoveKey == 0){
        return 0; /* not a movement key at all (e.g. Q) */
    }

    if(isWalkable(maze, newRow, newCol)){
        *playerRow = newRow;
        *playerCol = newCol;
        return 1;
    }

    return 0; /* movement key, but a wall was in the way */
}

/* ============================================================
   playGame
   The main game loop. Generates a new maze for every level,
   reads player input, applies movement rules, checks for the
   win condition, and keeps score for the whole session.
   ============================================================ */
int playGame(char name[]){
    char maze[ROWS][COLS];
    int level = 1;
    int totalMoves = 0;
    int totalScore = 0;
    int playing = 1;

    while(playing == 1){
        drawBorders(maze);
        generateMaze(maze);

        int playerRow = 1;
        int playerCol = 1;
        int exitRow = ROW_MAX - 1;
        int exitCol = COL_MAX - 1;
        maze[exitRow][exitCol] = EXIT_CHAR;

        int moves = 0;
        int won = 0;
        int quit = 0;

        while(won == 0 && quit == 0){
            clearScreen();
            printf("Player: %s   Score: %d\n", name, totalScore);
            printf("LEVEL: %d of %d   MOVES THIS LEVEL: %d   TOTAL MOVES: %d\n", level, MAX_LEVELS, moves, totalMoves);
            printf("Reach the '%c' to win the level!  (W/A/S/D to move, Q to quit)\n\n", EXIT_CHAR);

            displayMaze(maze, playerRow, playerCol);

            int input = getInputChar();

            if(input == EOF || input == 'q' || input == 'Q'){
                quit = 1;
            }
            else{
                int moved = tryMove(maze, &playerRow, &playerCol, input);

                if(moved == 1){
                    moves++;
                    totalMoves++;

                    if(playerRow == exitRow && playerCol == exitCol){
                        won = 1;
                    }
                }
                /* any other key (including a blocked move) is
                   ignored and the loop just redraws */
            }
        }

        if(quit == 1){
            playing = 0;
        }
        else{
            int mazeScore = calculateMazeScore(level, moves);
            totalScore = totalScore + mazeScore;

            if(level == MAX_LEVELS){
                /* cleared the final maze - the game is won, no more rounds */
                clearScreen();
                printf("============================================================\n");
                printf("                      YOU WIN!\n");
                printf("============================================================\n\n");
                printf(" Congratulations, %s! You cleared all %d levels!\n", name, MAX_LEVELS);
                printf(" Your score for this final maze: %d points\n", mazeScore);
                printf(" Total moves used: %d\n", totalMoves);
                printf(" FINAL SCORE: %d points\n", totalScore);
                printf("============================================================\n\n");
                printf("Press any key to return to the main menu...");
                getInputChar();

                return totalScore;
            }

            clearScreen();
            printf("============================================================\n");
            printf(" Congratulations, %s! You reached the exit in %d moves.\n", name, moves);
            printf(" Your score for this maze: %d points\n", mazeScore);
            printf(" Total score so far: %d points\n", totalScore);
            printf(" Advancing to level %d of %d with a brand new maze...\n", level + 1, MAX_LEVELS);
            printf("============================================================\n\n");
            printf("Press any key to continue, or Q to stop playing: ");

            int next = getInputChar();
            printf("\n");

            if(next == 'q' || next == 'Q' || next == EOF){
                playing = 0;
            }
            else{
                level++;
            }
        }
    }

    clearScreen();
    printf("============================================================\n");
    printf(" GAME OVER\n");
    printf(" Player: %s\n", name);
    printf(" You reached level %d with a total of %d moves.\n", level, totalMoves);
    printf(" FINAL SCORE: %d points\n", totalScore);
    printf("============================================================\n\n");
    printf("Press any key to return to the main menu...");
    getInputChar();

    return totalScore;
}

/* ============================================================
   main
   ============================================================ */
int main(void){
    srand((unsigned int) time(NULL));

    char playerName[MAX_NAME_LEN];

    showWelcomeScreen();
    getPlayerName(playerName);

    int running = 1;
    int lastScore = 0;
    while(running == 1){
        int choice = showMainMenu(playerName, lastScore);

        if(choice == 1){
            lastScore = playGame(playerName);
        }
        else if(choice == 2){
            showInstructions();
        }
        else if(choice == 3){
            clearScreen();
            printf("Thanks for visiting 2D Maze Explorer, %s. Goodbye!\n", playerName);
            running = 0;
        }
        else{
            printf("\nInvalid option. Press any key to try again...");
            getInputChar();
        }
    }

    return 0;
}
