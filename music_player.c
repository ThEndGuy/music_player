#include <stdio.h>
#include <stdbool.h>
#include "include/raylib.h"

int main(int argc, char** argv){
    (void) argc;
    (void) argv;

    InitWindow(800, 600, "C window");
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){0,0,10,255});
        EndDrawing();
    }
    CloseWindow();
    return 0;

}
