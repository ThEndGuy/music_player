#include <stdbool.h>
#include <stdio.h>

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

const char *token_by_name(Token_Type type) {
    switch (type) {
    case TOKEN_EOF:
        return "TOKEN_EOF";
    case TOKEN_VAR:
        return "TOKEN_VAR";
    case TOKEN_STR:
        return "TOKEN_STR";
    case TOKEN_INT:
        return "TOKEN_INT";
    case TOKEN_DOT:
        return "TOKEN_DOT";
    case TOKEN_COLON:
        return "TOKEN_COLON";
    case TOKEN_FLOAT:
        return "TOKEN_FLOAT";
    case TOKEN_OPAREN:
        return "TOKEN_OPAREN";
    case TOKEN_CPAREN:
        return "TOKEN_CPAREN";
    case TOKEN_COMMA:
        return "TOKEN_COMMA";
    case TOKEN_EQUAL:
        return "TOKEN_EQUAL";
    case TOKEN_INDENT:
        return "TOKEN_INDENT";
    case TOKEN_UNINDENT:
        return "TOKEN_UNINDENT";
    case TOKEN_NL:
        return "TOKEN_NL";
    case TOKEN_UNDEFINED:
        return "TOKEN_UNDEFINED";
    default:
        return "UNKNOWN";
    }
}

typedef struct {
    Token_Type type;
    String_View sv;
} Token;

typedef struct {
    whitespace_type type;
    size_t count;
} Indent;

typedef struct {
    int line;
    int col;
    String_View sv;
    char *file;
    bool at_start;
    Indent exp_ind;

    Indent last_ind;
} Lexer;

typedef enum {
    NODE_ASSIGNMENT,
} Node_Type;

typedef struct {
    String_View *items;
    size_t count;
    size_t capacity;
} Reference;

typedef struct Node {
    String_View key;
    union {
        String_View sv;
        int integer;
        Reference ref;
    } as;

    struct Node *items; // children
    size_t count;
    size_t capacity;
} Node;

typedef struct {
    Lexer lexer;
    Token current;
} Parser;

int sv_to_int(String_View sv) {
    int sign = 1;
    int result = 0;
    size_t i = 0;
    if (sv.count > 0 && sv.data[0] == '-') { // Negative number
        sign = -1;
        i++;
    }
    for (; i < sv.count; i++) {
        result = result * 10 + (sv.data[i] - '0');
    }
    return result * sign;
}

void error(char *file_name, int line, int col, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    //nob_log(NOB_ERROR, "%s:%d:%d: test ", file_name, line, col);
    fprintf(stderr, "%s:%d:%d: ", file_name, line, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void l_error(Lexer l, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    //nob_log(NOB_ERROR, "%s:%d:%d: test ", file_name, line, col);
    fprintf(stderr, "%s:%d:%d: ", l.file, l.line, l.col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}


Lexer lexer_init(char *file_name) {
    String_Builder sb = {0};
    read_entire_file(file_name, &sb);
    String_View sv = sb_to_sv(sb);
    return (Lexer){.line = 1,
                   .col = 1,
                   .sv = sv,
                   .file = file_name,
                   .exp_ind = (Indent){WS_UNSET, 0},
                   .at_start = true};
}

bool lexer_check_whitespace(Lexer *l, whitespace_type ws, size_t length) {
    if (!l->at_start) {
        return false;
    }
    l->at_start = false;
    if (l->exp_ind.type != WS_UNSET) {
        if (l->exp_ind.type != ws || length % l->exp_ind.count != 0) {
            return false;
        } else
            return true;
    } else {
        l->exp_ind = (Indent){.type = ws, .count = length};
        return true;
    }
}

Token lexer_next_token(Lexer *l) {
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
        } else if (c == '\t') {
            ws = WS_TAB;
            sv_chop_left(&l->sv, 1);
            length++;
            c = l->sv.data[0];
        } else if (c == '\r') { // windows carriage return..
            sv_chop_left(&l->sv, 1);
            c = l->sv.data[0];
        } else
            break;
    }
    // If there is a whitespace, handle it:
    if (length > 0) {
        l->col += length;
        if (lexer_check_whitespace(l, ws, length)) {
            if (l->last_ind.count < length) {
                l->last_ind = (Indent){.type = ws, .count = length};
                return (Token){.type = TOKEN_INDENT, .sv = sv_from_cstr("->")};
            } else if (l->last_ind.count > length) {
                l->last_ind = (Indent){.type = ws, .count = length};
                return (Token){.type = TOKEN_UNINDENT,
                               .sv = sv_from_cstr("<-")};
            }
        }
    }
    length = 0;
    const char *start = l->sv.data;
    if (isdigit(c)) {
        Token_Type tt = TOKEN_INT;
        while (true) {
            sv_chop_left(&l->sv, 1);
            length++;
            c = l->sv.data[0];
            if (c == '.')
                tt = TOKEN_FLOAT;
            else if (!isdigit(c))
                break;
        }
        l->col += length;
        String_View number = {.data = start, .count = length};
        return (Token){.type = tt, .sv = number};
    } else if (c == '-' && isdigit(l->sv.data[1])) { // The next must be a number
        Token_Type tt = TOKEN_INT;
        while (true) {
            sv_chop_left(&l->sv, 1);
            length++;
            c = l->sv.data[0];
            if (c == '.')
                tt = TOKEN_FLOAT;
            else if (!isdigit(c))
                break;
        }
        l->col += length;
        String_View number = {.data = start, .count = length};
        return (Token){.type = tt, .sv = number};
    }
    if (isalpha(c)) {
        while (true) {
            sv_chop_left(&l->sv, 1);
            length++;
            c = l->sv.data[0];
            if (!isalpha(c) && !isdigit(c))
                break;
        }
        String_View variable = {
            .data = start,
            .count = length,
        };
        l->col += length;
        return (Token){.type = TOKEN_VAR, .sv = variable};
    } else if (c == '=') {
        length += 1;
        sv_chop_left(&l->sv, 1);
        String_View eq = {
            .data = start,
            .count = length,
        };
        l->col += length;
        return (Token){.type = TOKEN_EQUAL, .sv = eq};

    } else if (c == '\n') {
        sv_chop_left(&l->sv, 1);
        l->at_start = true;
        l->line += 1;
        l->col = 1;
        return (Token){.type = TOKEN_NL, .sv = sv_from_cstr("\\n")};
    } else if (c == ':') {
        sv_chop_left(&l->sv, 1);
        l->col += 1;
        return (Token){.type = TOKEN_COLON, .sv = sv_from_cstr(":")};
    } else if (c == '(') {
        sv_chop_left(&l->sv, 1);
        l->col += 1;
        return (Token){.type = TOKEN_OPAREN, .sv = sv_from_cstr("(")};
    } else if (c == ')') {
        sv_chop_left(&l->sv, 1);
        l->col += 1;
        return (Token){.type = TOKEN_CPAREN, .sv = sv_from_cstr(")")};
    } else if (c == '"') {
        sv_chop_left(&l->sv, 1);
        length++;
        c = l->sv.data[0];
        while (c != '"') {
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
        l->col += length;
        return (Token){.type = TOKEN_STR, .sv = str};

    } else if (c == COMMENT_CHAR) {
        while (true) {
            if (l->sv.count == 0) {
                return (Token){.type = TOKEN_EOF, .sv = sv_from_cstr("EOF")};
            }
            sv_chop_left(&l->sv, 1);
            c = l->sv.data[0];
            if (c == '\n') {
                sv_chop_left(&l->sv, 1);
                l->at_start = true;
                l->col = 1;
                l->line += 1;
                return (Token){.type = TOKEN_NL, .sv = sv_from_cstr("\\n")};
            }
        }
    } else if (c == '.') {
        sv_chop_left(&l->sv, 1);
        l->col += 1;
        return (Token){.type = TOKEN_DOT, .sv = sv_from_cstr(".")};
    } else {
        String_View sv = {.data = &l->sv.data[0], .count = 1};
        sv_chop_left(&l->sv, 1);
        l->col += 1;
        return (Token){.type = TOKEN_UNDEFINED, .sv = sv};
    }
}

void print_token(Token t) {
    printf(SV_Fmt " => %s\n", SV_Arg(t.sv), token_by_name(t.type));
}

void dump_tokens(Lexer *l) {
    Token t;
    do {
        t = lexer_next_token(l);
        print_token(t);
    } while (t.type != TOKEN_EOF);
}


Parser parser_init(char *file_name) {
    Parser parser = {
        .lexer = lexer_init(file_name),
    };
    parser.current = lexer_next_token(&parser.lexer);
    return parser;
}

Token parser_peek(Parser *p) { return p->current; }

Token parser_consume(Parser *p) {
    Token t = p->current;
    p->current = lexer_next_token(&p->lexer);
    return t;
}

Token parser_expect(Parser *p, Token_Type exp_tok) {
    Token token = parser_consume(p);
    if (token.type != exp_tok) {
        error(p->lexer.file, p->lexer.line, p->lexer.col,
              "Error while parsing: expected: %s  got: %s  ("SV_Fmt")",
              token_by_name(exp_tok), token_by_name(token.type), SV_Arg(token.sv));
    }
    return token;
}

void parse_tokens(Lexer *l) {
    Token t;
    while (t.type != TOKEN_EOF) {
        t = lexer_next_token(l);
        switch (t.type) {
        case TOKEN_INT: {
            printf("Parsed an int\n");
        } break;
        case TOKEN_VAR: {
            printf("Parsed a var\n");
        } break;
        default:
            error(__FILE__, __LINE__, 0, "Unhandled token: %s",
                  token_by_name(t.type));
        }
    }
}

Node parse_block(Parser *p, int indent_level) {
    Token t;
    Node n = {0};
    if (indent_level == 0) n.key = sv_from_cstr("MAIN");
    while (p->current.type != TOKEN_EOF && p->current.type != TOKEN_UNINDENT) {
        if (parser_peek(p).type == TOKEN_NL) {
            parser_consume(p);
            continue;
        }
        t = parser_expect(p, TOKEN_VAR);
        Token next = parser_peek(p);
        if (next.type == TOKEN_COLON) {
            parser_consume(p); // colon
            parser_expect(p, TOKEN_NL);
            parser_expect(p, TOKEN_INDENT);
            Node sub_n = parse_block(p, indent_level + 1);
            sub_n.key = t.sv;
            da_append(&n, sub_n);
        } else if (next.type == TOKEN_EQUAL) {
            parser_consume(p); // equal
            Token var = parser_consume(p); // whatever the assignment is
            switch (var.type) {
            case TOKEN_STR: {
                Node sub_n = {.key = t.sv, .as.sv = var.sv};
                da_append(&n, sub_n);
            } break;
            case TOKEN_INT: {
                Node sub_n = {.key = t.sv, .as.integer = sv_to_int(var.sv)};
                da_append(&n, sub_n);
            } break;
            case TOKEN_VAR: { // GLOBAL REFERENCE
                parser_expect(p, TOKEN_DOT);
                Reference ref = {0};
                da_append(&ref, var.sv);
                while (true) {
                    Token v = parser_expect(p, TOKEN_VAR);
                    da_append(&ref, v.sv);
                    if (parser_consume(p).type != TOKEN_DOT) break;
                }
                Node sub_n = {.key = t.sv, .as.ref = ref};
                da_append(&n, sub_n);
            } break;
            case TOKEN_DOT: { // LOCAL REFERENCE
                Reference ref = {0};
                da_append(&ref, var.sv);
                while (true) {
                    Token v = parser_expect(p, TOKEN_VAR);
                    da_append(&ref, v.sv);
                    if (parser_consume(p).type != TOKEN_DOT) break;
                }
                Node sub_n = {.key = t.sv, .as.ref = ref};
                da_append(&n, sub_n);
            } break;
            default: {
                l_error(p->lexer, "Unknown token: %s ("SV_Fmt")", token_by_name(t.type), SV_Arg(t.sv));
            } break;

            }
        } else {
            l_error(p->lexer, "Unknown token: %s ("SV_Fmt")", token_by_name(t.type), SV_Arg(t.sv));
        }
    }
    return n;
}

typedef struct {
    Node *node;
    bool is_valid;
} Node_Ret;

Node_Ret node_find_child(Node *parent, String_View key) {
    da_foreach(Node, it, parent) {
        if (nob_sv_eq(it->key, key)) {
            return (Node_Ret){.node = it, .is_valid = true};
        }
    }
    return (Node_Ret){.is_valid = false};
}

int main(void) {

    Parser p = parser_init("./idoc/tests/simple_test.idoc");
    Node n = parse_block(&p, 0);
    // for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)

    Node_Ret nr = node_find_child(&n, sv_from_cstr("Testing"));
    if (!nr.is_valid) nob_log(NOB_ERROR, "Could not find child");
    printf(SV_Fmt"\n", SV_Arg(nr.node->key));

    printf(SV_Fmt"\n", SV_Arg(n.key));
    printf(SV_Fmt"\n", SV_Arg(n.items[0].key));
    printf(SV_Fmt"\n", SV_Arg(n.items[0].items[0].as.sv));

    return 0;
}
