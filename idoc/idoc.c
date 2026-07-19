#include <stdio.h>
#include <stdbool.h>

#define NOB_IMPLEMENTATION
#include "../include/nob.h"

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
    TOKEN_OPAREN,
    TOKEN_CPAREN,
    TOKEN_COMMA,
    TOKEN_EQUAL,
    TOKEN_INDENT,
    TOKEN_UNINDENT,
    TOKEN_UNDEFINED,
} Token_Type;

const char *token_by_name(Token_Type type)
{
    switch (type) {
    case TOKEN_EOF:    return "TOKEN_EOF";
    case TOKEN_VAR:    return "TOKEN_VAR";
    case TOKEN_STR:    return "TOKEN_STR";
    case TOKEN_INT:    return "TOKEN_INT";
    case TOKEN_DOT:    return "TOKEN_DOT";
    case TOKEN_COLON:  return "TOKEN_COLON";
    case TOKEN_FLOAT:  return "TOKEN_FLOAT";
    case TOKEN_OPAREN: return "TOKEN_OPAREN";
    case TOKEN_CPAREN: return "TOKEN_CPAREN";
    case TOKEN_COMMA:  return "TOKEN_COMMA";
    case TOKEN_EQUAL:  return "TOKEN_EQUAL";
    case TOKEN_INDENT: return "TOKEN_INDENT";
    case TOKEN_UNINDENT: return "TOKEN_UNINDENT";
    case TOKEN_UNDEFINED: return "TOKEN_UNDEFINED";

    default:           return "UNKNOWN";
    }
}

typedef struct {
    Token_Type type;
    String_View sv;
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
    String_View sv;
    char* file;
    Indent exp_ind;

    Tokens tkns;
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
      .sv = sv,
      .file = file_name,
      .exp_ind = (Indent){UNSET, 0}
    };
}

void strip_comment(String_View *sv) {
    *sv = sv_chop_by_delim(sv, COMMENT_CHAR);
}



Token lexer_next_token(Lexer *l){
    // TODO:  Add indentation count support
    size_t length = 0;
    char c = l->sv.data[0];
    while (isspace(c)) {
        sv_chop_left(&l->sv, 1); // chop them spaces
        c = l->sv.data[0];
    }
    const char* start = l->sv.data;
    if (isdigit(c)) {
        Token_Type tt = TOKEN_INT;
        while (true) {
            sv_chop_left(&l->sv, 1);
            length++;
            c = l->sv.data[0];
            if (c == '.') tt = TOKEN_FLOAT;
            else if (!isdigit(c)) break;
        }
        String_View number = {
            .data = start,
            .count = length
        };
        return (Token){.type = tt, .sv = number};
    }
    else if (isalpha(c)){
        while (true) {
            sv_chop_left(&l->sv, 1);
            length++;
            c = l->sv.data[0];
            if (!isalpha(c) && !isdigit(c)) break;
        }
        String_View variable = {
            .data = start,
            .count = length,
        };
        return (Token){.type = TOKEN_VAR, .sv = variable };
    }
    else if (c == '='){
        sv_chop_left(&l->sv, 1);
        String_View eq = {
            .data = start,
            .count = 1,
        };
        return (Token){.type = TOKEN_EQUAL, .sv = eq };

    }
    else if (c == '\n') {
        sv_chop_left(&l->sv, 1);
        return (Token){.type = TOKEN_INDENT, .sv = sv_from_cstr("\n")};
    }
    else {
        String_View sv = {.data = &l->sv.data[0], .count = 1};
        sv_chop_left(&l->sv, 1);
        return (Token){.type = TOKEN_UNDEFINED, .sv = sv};
    }
}

/* Line lexer_next_line(Lexer *l){ */
/*     Line line = {0}; */
/*     line.sv = sv_chop_by_delim(&l->data, '\n'); */
/*     sv_chop_suffix(&line.sv, sv_from_cstr("\r")); */
/*     strip_comment(&line.sv); */
/*     line.ind = (Indent){.type = NO_INDENT, .count = 0}; */
/*     if (line.sv.count == 0) { */
/*         l->line++; */
/*         return line; */
/*     } */
/*     char c = line.sv.data[0]; */
/*     while (c == TAB || c == SPACE) { */
/*         if (line.ind.type == NO_INDENT){ */
/*             line.ind.type = c; */
/*         } else if (l->exp_ind.type != UNSET && l->exp_ind.type != c) { */
/*             error(l->file, l->line, line.ind.count, */
/*                   "Inconsistent indentation is not suported."  */
/*                   "Use only spaces or only tabs"); */
/*             exit(1); */
/*         } */
/*         c = line.sv.data[++line.ind.count]; */
/*     } */
/*     if (l->exp_ind.type == UNSET && line.ind.type != NO_INDENT) { */
/*         l->exp_ind = line.ind; */
/*     } */
/*     if (l->exp_ind.type != UNSET && line.ind.count % l->exp_ind.count != 0) { */
/*         error(l->file, l->line, line.ind.count, "Wrong indentation level"); */
/*         exit(1); */
/*     } */
/*     sv_chop_left(&line.sv, line.ind.count); */
/*     line.level = (int) ((float)line.ind.count / l->exp_ind.count); */
/*     l->line++; */
/*     return line; */
/* } */
void parse_line(Line *line, Tokens *tkns){
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
            Token n = {.type = tt, .sv = number};
            da_append(tkns, n);
        }
        else if (isspace(c)){
            continue;
        }
        else if (isalpha(c)){
            int start = i;
            while (i < line->sv.count) {
                char c2 = line->sv.data[i];
                if (isalpha(c2) || isdigit(c2)) {
                        i++;
                    } else {
                        break;
                    }
            }
            String_View variable = {
                .data = line->sv.data + start,
                .count = i - start,
            };
            Token s = {.type = TOKEN_VAR, .sv = variable };
            da_append(tkns, s);
        }
        else if (c == '='){
            String_View eq = {
                .data = line->sv.data + i,
                .count = 1,
            };
            Token s = {.type = TOKEN_EQUAL, .sv = eq };
            da_append(tkns, s);

        }
        else {}
    }
}

/* void parse_lines(Lexer *l, Tokens *tkns) { */

/*     while (l->sv.count > 0) { */
/*         Line line = lexer_next_line(l); */
/*         parse_line(&line, tkns); */
/*     } */
/* } */

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
    Tokens tkns = {0};
    Lexer l = lex_init("./idoc/tests/test.idoc");
    while (l.sv.count > 0){
        Token t = lexer_next_token(&l);
        printf(SV_Fmt" => %s\n", SV_Arg(t.sv), token_by_name(t.type));
    }

    return 0;

}
