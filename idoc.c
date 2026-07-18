#include <stdio.h>
#include <stdbool.h>

#define NOB_IMPLEMENTATION
#include "./include/nob.h"

#define COMMENT_CHAR '#'

typedef enum {
    UNSET = -1,
    NO_INDENT,
    TAB = '\t',
    SPACE = ' ',
} whitespace_type;

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_VAR,
    TOKEN_STR,
    TOKEN_INT,
    TOKEN_DOT,
    TOKEN_COLON,
    TOKEN_FLOAT,
} Token_Type;

typedef struct {
    Token_Type type;
    String_View value;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} Tokens;

typedef struct {
    whitespace_type type;
    int count;
} Indent;

typedef struct {
    Indent ind;
    String_View sv;
    int level;
} Line;

typedef struct {
    int line;
    String_View data;
    char* file;
    Indent exp_ind;
} Lexer;


void error(char* file_name, int line, int col, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    nob_log(NOB_ERROR, "%s:%d:%d: ", file_name, line, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

Lexer lex_init(char* file_name){
    String_Builder sb = {0};
    read_entire_file(file_name , &sb);
    String_View sv = sb_to_sv(sb);
    return (Lexer){
      .line = 0,
      .data = sv,
      .file = file_name,
      .exp_ind = (Indent){UNSET, 0}
    };
}

void strip_comment(String_View *sv) {
    *sv = sv_chop_by_delim(sv, COMMENT_CHAR);
}


Line lexer_next_line(Lexer *l){
    Line line = {0};
    line.sv = sv_chop_by_delim(&l->data, '\n');
    sv_chop_suffix(&line.sv, sv_from_cstr("\r"));
    strip_comment(&line.sv);
    line.ind = (Indent){.type = NO_INDENT, .count = 0};
    if (line.sv.count == 0) {
        l->line++;
        return line;
    }
    char c = line.sv.data[0];
    while (c == TAB || c == SPACE) {
        if (line.ind.type == NO_INDENT){
            line.ind.type = c;
        } else if (l->exp_ind.type != UNSET && l->exp_ind.type != c) {
            error(l->file, l->line, line.ind.count,
                  "Inconsistent indentation is not suported." 
                  "Use only spaces or only tabs");
            exit(1);
        }
        c = line.sv.data[++line.ind.count];
    }
    if (l->exp_ind.type == UNSET && line.ind.type != NO_INDENT) {
        l->exp_ind = line.ind;
    }
    if (l->exp_ind.type != UNSET && line.ind.count % l->exp_ind.count != 0) {
        error(l->file, l->line, line.ind.count, "Wrong indentation level");
        exit(1);
    }
    sv_chop_left(&line.sv, line.ind.count);
    line.level = (int) ((float)line.ind.count / l->exp_ind.count);
    l->line++;
    return line;
}
Tokens parse_line(Line *line){
    Tokens tkns = {0};
    for (size_t i = 0; i < line->sv.count; i++) {
        char c = line->sv.data[i];
        if (isdigit(c)) {
            int start = i;
            Token_Type tt = TOKEN_INT;
            while (i < line->sv.count) {
                char c2 = line->sv.data[i];
                    if (isdigit(c2)) {
                        i++;
                        continue;
                    } else if (c2 == '.') {
                        i++;
                        tt = TOKEN_FLOAT;
                    } else {
                        break;
                    }
            }
            String_View number = {
                .data = line->sv.data + start,
                .count = i - start,
            };
            Token n = {.type = tt, .value = number};
            da_append(&tkns, n);
            i--;
        }
        else if (isspace(c)){
            continue;
        }
        else {TODO("Not implemented yet");}

    }
    return tkns;
}

void parse_lines(Lexer *l) {


    while (l->data.count > 0) {
        Line line = lexer_next_line(l);
        Tokens tkns = parse_line(&line);
        (void) tkns;
    }

}

Line __debug_line(char* text){
    Line line = (Line){
        .ind = (Indent){' ', 1},
        .sv = sv_from_cstr(text),
        .level = 0
    };
    return line;
}

int main(int argc, char** argv){
    (void) argc;
    (void) argv;
    Line line = __debug_line("1234 a 12.34");
    Tokens tks = parse_line(&line);
    printf("tokens: %zu\n", tks.count);
    for (size_t i = 0; i < tks.count; i++){
        Token t = tks.items[i];
        printf(SV_Fmt" = %d\n", SV_Arg(t.value), t.type);
    }

    exit(0);
    Lexer l = lex_init("./test.idoc");
    while (l.data.count > 0) {
        Line line = lexer_next_line(&l);
        printf("%d:", l.line);
        for (int i = 0; i < line.ind.count; i++) {printf(" ");}
        printf("'"SV_Fmt"' ('%c':%d level:%d)\n",
               SV_Arg(line.sv),
               line.ind.type,
               line.ind.count,
               line.level
               );
    }

    return 0;

}
