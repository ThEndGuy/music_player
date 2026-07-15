#define NOB_IMPLEMENTATION

#include ".include/nob.h"

#define BUILD_FOLDER "./build/"

int main(int argc, char** argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    

    shift(argv, argc);
    Cmd cmd = {0};
    cmd_append(&cmd, "cc", "-Wall", "-Wextra");
    cmd_append(&cmd, "-Iinclude","-Llib");

    cmd_append(&cmd, "-o", BUILD_FOLDER"music_player", "music_player.c");
    // linking
    cmd_append(&cmd, "-lraylib", "-lopengl32", "-lgdi32", "-lwinmm");
    if (!cmd_run(&cmd)) return 1;
    cmd.count = 0;

    if (argc > 0) {
        char* flag = argv[0];
        if (strcmp(flag, "run") == 0) {
            cmd_append(&cmd, BUILD_FOLDER"music_player");
            if (!cmd_run(&cmd)) return 1;
        }
    }

    return 0;
}


