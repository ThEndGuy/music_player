#define NOB_IMPLEMENTATION
#include "./include/nob.h"

#define BUILD_FOLDER "./build/"
#define IDOC_FOLDER "./idoc/"
#define LIB_FOLDER "./lib/"

void usage(){
    printf("Usage:\n");
    printf("  nob help - print this message\n");
    printf("  nob      - build all\n");
    printf("  nob idoc - build and run idoc\n");
    printf("  nob mp   - build and run music_player\n");

}

int build_idoc(Cmd *cmd){
    cmd_append(cmd, "cc", "-ggdb", "-Wall", "-Wextra");
    cmd_append(cmd, "-o", BUILD_FOLDER"idoc", IDOC_FOLDER"idoc.c");
    if (!cmd_run(cmd)) return false;
    return true;
}

int build_music_player(Cmd *cmd){
    cmd_append(cmd, "cc", "-Wall", "-Wextra");
    cmd_append(cmd, "-Iinclude");

    cmd_append(cmd, "-o", BUILD_FOLDER"music_player", "music_player.c");
    // linking
#ifdef _WIN32
    cmd_append(cmd, "-Llib/windows");
    cmd_append(cmd, "-lraylib", "-lopengl32", "-lgdi32", "-lwinmm");
#else
    // You gotta tweak this if it doesnt work
    cmd_append(cmd, "-lraylib", "-lm", "-lpthread", "-ldl", "-lX11");
#endif
    if (!cmd_run(cmd)){
#ifdef _WIN32
#else
        nob_log(NOB_ERROR, "Make sure you have raylib installed!");
#endif
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    char *exe_path = temp_running_executable_path();
    char *exe_dir = temp_dir_name(exe_path);
    if (!set_current_dir(exe_dir)) return 1;
    argv[0] = exe_path;
    NOB_GO_REBUILD_URSELF(argc, argv);
    shift(argv, argc);
    Cmd cmd = {0};

    if (argc > 0) {
        char* flag = argv[0];
        shift(argv, argc);
        if (strcmp(flag, "mp") == 0) {
            if (!build_music_player(&cmd)) return 1;
            cmd_append(&cmd, BUILD_FOLDER"music_player");
            da_append_many(&cmd, argv, argc);
            if (!cmd_run(&cmd)) return 1;
        }
        else if (strcmp(flag, "idoc") == 0) {
            if (!build_idoc(&cmd)) return 1;
            cmd_append(&cmd, BUILD_FOLDER"idoc");
            da_append_many(&cmd, argv, argc);
            if (!cmd_run(&cmd)) return 1;
        }
        else if (strcmp(flag, "help") == 0) {
            usage();
            exit(0);
        }
        else {
            usage();
            nob_log(NOB_ERROR, "Unknown command `%s`", flag);
            exit(1);
        }
    } else {
        if (!build_music_player(&cmd)) return 1;
        if (!build_idoc(&cmd)) return 1;
    }

    return 0;
}
