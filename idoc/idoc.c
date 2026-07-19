#include <stdio.h>
#include <stdbool.h>

#define NOB_IMPLEMENTATION
#include "../include/nob.h"

#define COMMENT_CHAR '#'

typedef enum {
    WS_UNSET = -1,
    WS_NO_INDENT,
    WS_TAB = '\t',
    WS_SPACE = ' ',
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
    TOKEN_NL,
    TOKEN_UNDEFINED,
} Token_Type;

const char *token_by_name(Token_Type type)
{
    switch (type) {
    case TOKEN_EOF:       return "TOKEN_EOF";
    case TOKEN_VAR:       return "TOKEN_VAR";
    case TOKEN_STR:       return "TOKEN_STR";
    case TOKEN_INT:       return "TOKEN_INT";
    case TOKEN_DOT:       return "TOKEN_DOT";
    case TOKEN_COLON:     return "TOKEN_COLON";
    case TOKEN_FLOAT:     return "TOKEN_FLOAT";
    case TOKEN_OPAREN:    return "TOKEN_OPAREN";
    case TOKEN_CPAREN:    return "TOKEN_CPAREN";
    case TOKEN_COMMA:     return "TOKEN_COMMA";
    case TOKEN_EQUAL:     return "TOKEN_EQUAL";
    case TOKEN_INDENT:    return "TOKEN_INDENT";
    case TOKEN_UNINDENT:  return "TOKEN_UNINDENT";
    case TOKEN_NL:        return "TOKEN_NL";
    case TOKEN_UNDEFINED: return "TOKEN_UNDEFINED";
    default:              return "UNKNOWN";
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
    size_t count;
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
    bool at_start;
    Indent exp_ind;

    Indent last_ind;

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
    exit(1);
}

Lexer lex_init(char* file_name){
    String_Builder sb = {0};
    read_entire_file(file_name , &sb);
    String_View sv = sb_to_sv(sb);
    return (Lexer){
      .line = 0,
      .sv = sv,
      .file = file_name,
      .exp_ind = (Indent){WS_UNSET, 0},
      .at_start = true
    };
}


bool lexer_check_whitespace(Lexer *l, whitespace_type ws, size_t length){
    if (!l->at_start){
        return false;
        }
    l->at_start = false;
    if (l->exp_ind.type != WS_UNSET){
        if (l->exp_ind.type != ws || length % l->exp_ind.count != 0) {
            return false;
        } else return true;
    } else {
        l->exp_ind = (Indent){.type = ws, .count = length};
        return true;
    }
}

Token lexer_next_token(Lexer *l){
    if (l->sv.count == 0) {
        return (Token){.type = TOKEN_EOF, .sv = sv_from_cstr("EOF")};
    }
    size_t length = 0;
    char c = l->sv.data[0];
    whitespace_type ws;
    while (true) { // Whitespace handling
        if (c == ' ') {
            ws = WS_SPACE;
            sv_chop_left(&l->sv, 1);
            length++;
            c = l->sv.data[0];
        } else if (c == '\t'){
            ws = WS_TAB;
            sv_chop_left(&l->sv, 1);
            length++;
            c = l->sv.data[0];
        } else if (c == '\r'){ // windows carriage return..
            sv_chop_left(&l->sv, 1);
            c = l->sv.data[0];
        } else break;
    }
    if (length > 0) {
        if (lexer_check_whitespace(l, ws, length)) {
            if (l->last_ind.count < length) {
                l->last_ind = (Indent){.type = ws, .count = length};
                return (Token){
                    .type = TOKEN_INDENT,
                    .sv = sv_from_cstr("->")
                };
            } else if (l->last_ind.count > length) {
                l->last_ind = (Indent){.type = ws, .count = length};
                return (Token){
                    .type = TOKEN_UNINDENT,
                    .sv = sv_from_cstr("<-")
                };
            }
        }
    }
    length = 0;
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
        l->at_start = true;
        return (Token){.type = TOKEN_NL, .sv = sv_from_cstr("\\n")};
    }
    else if (c == ':') {
        sv_chop_left(&l->sv, 1);
        return (Token){.type = TOKEN_COLON, .sv = sv_from_cstr(":")};
    }
    else if (c == '(') {
        sv_chop_left(&l->sv, 1);
        return (Token){.type = TOKEN_OPAREN, .sv = sv_from_cstr("(")};
    }
    else if (c == ')') {
        sv_chop_left(&l->sv, 1);
        return (Token){.type = TOKEN_CPAREN, .sv = sv_from_cstr(")")};
    }
    else if (c == '"') {
        sv_chop_left(&l->sv, 1);
        length++;
        c = l->sv.data[0];
        while (c != '"'){
            sv_chop_left(&l->sv, 1);
            if (l->sv.count == 0) {
                error(l->file, l->line, length, "Unbalanced '\"'.");
            }
            length++;
            c = l->sv.data[0];
        }
        sv_chop_left(&l->sv, 1);
        length++;
        String_View str = {
            .data = start,
            .count = length,
        };
        return (Token){.type = TOKEN_STR, .sv = str };

    }
    else if (c == COMMENT_CHAR){
        while (true){
            if (l->sv.count == 0) {
                return (Token){.type = TOKEN_EOF, .sv = sv_from_cstr("EOF")};
            }
            sv_chop_left(&l->sv, 1);
            c = l->sv.data[0];
            if (c == '\n') {
                sv_chop_left(&l->sv, 1);
                l->at_start = true;
                return (Token){.type = TOKEN_NL, .sv = sv_from_cstr("\\n")};
            }
        }
    }
    else if (c == '.') {
        sv_chop_left(&l->sv, 1);
        return (Token){.type = TOKEN_DOT, .sv = sv_from_cstr(".")};
    }
    else {
        String_View sv = {.data = &l->sv.data[0], .count = 1};
        sv_chop_left(&l->sv, 1);
        return (Token){.type = TOKEN_UNDEFINED, .sv = sv};
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
void dump_tokens(Lexer *l){
    Token t;
    do {
        t = lexer_next_token(l);
        printf(SV_Fmt" => %s\n", SV_Arg(t.sv), token_by_name(t.type));
    } while (t.type != TOKEN_EOF);
}

void parse_tokens(Lexer *l) {
    Token t;
    while (t.type != TOKEN_EOF){
        t = lexer_next_token(l);
        switch (t.type) {
        case TOKEN_INT: {
            printf("Parsed an int\n");
        } break;
        case TOKEN_VAR: {
            printf("Parsed a var\n");
        } break;
        default:
            error(__FILE__, __LINE__, 0,
                  "Unhandled token: %s", token_by_name(t.type));
        }
    }
}

int main(int argc, char** argv){
    (void) argc;
    (void) argv;
    Lexer l = lex_init("./idoc/tests/test.idoc");
    //dump_tokens(&l);
    //parse_tokens(&l);

    return 0;

}
