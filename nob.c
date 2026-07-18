

#define NOB_IMPLEMENTATION
#include "./include/nob.h"

#define BUILD_FOLDER "./build/"

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    // for (int i = 0; i < argc; i++) {
    //     printf("%d = %s\n", i, argv[i]);
    // }
    shift(argv, argc);
    Cmd cmd = {0};
    cmd_append(&cmd, "cc", "-Wall", "-Wextra");
    cmd_append(&cmd, "-Iinclude","-Llib");

    cmd_append(&cmd, "-o", BUILD_FOLDER"music_player", "music_player.c");
    // linking
#ifdef _WIN32
    cmd_append(&cmd, "-lraylib", "-lopengl32", "-lgdi32", "-lwinmm");
#else
    // You gotta tweak this if it doesnt work
    cmd_append(&cmd, "lraylib", "-lm", "-lpthread", "-ldl", "-lX11");
#endif
    if (!cmd_run(&cmd)) return 1;
    cmd.count = 0;

    cmd_append(&cmd, "cc", "-Wall", "-Wextra");
    cmd_append(&cmd, "-o", BUILD_FOLDER"idoc", "idoc.c");
    if (!cmd_run(&cmd)) return 1;

    cmd.count = 0;
    if (argc > 0) {
        char* flag = argv[0];
        shift(argv, argc);
        if (strcmp(flag, "mp") == 0) {
            cmd_append(&cmd, BUILD_FOLDER"music_player");
            da_append_many(&cmd, argv, argc);
            if (!cmd_run(&cmd)) return 1;
        }
        else if (strcmp(flag, "idoc") == 0) {
            cmd_append(&cmd, BUILD_FOLDER"idoc");
            da_append_many(&cmd, argv, argc);
            if (!cmd_run(&cmd)) return 1;
        }
        else {
            nob_log(NOB_ERROR, "Unknown command %s", flag);
            exit(1);
        }
    }

    return 0;
}


