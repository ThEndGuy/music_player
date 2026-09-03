#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

// #define NOB_IMPLEMENTATION
// #include "../include/nob.h"

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
    char *items;
    size_t count;
    size_t capacity;
} Idoc_SB;

typedef struct {
    size_t count;
    const char *data;
} Idoc_SV;

typedef struct {
    Token_Type type;
    Idoc_SV sv;
} Token;

typedef struct {
    whitespace_type type;
    size_t count;
} Indent;

typedef struct {
    int line;
    int col;
} Loc;

typedef struct {
    Loc loc;
    Idoc_SV sv;
    const char *file;
    bool at_start;
    Indent exp_ind;

    Indent last_ind;
} Lexer;


typedef enum {
    VALUE_EMPTY,
    VALUE_STRING,
    VALUE_INTEGER,
    VALUE_FLOAT,
    VALUE_TUPLE,
    VALUE_REFERENCE,
} Value_Type;

typedef enum {
    NODE_BLOCK,
    NODE_STRING,
    NODE_INTEGER,
    NODE_REFERENCE,
} Node_Type;

typedef struct Value Value;

struct Value {
    Value_Type type;
    union {
        Idoc_SV string;
        int integer;
        double floating;
        struct {
            Idoc_SV *items;
            size_t count;
            size_t capacity;
        } ref;
        struct {
            Value *items;
            size_t count;
            size_t capacity;
        } tuple;
    };
};

typedef struct Node {
    Idoc_SV key;
    Value value;
    Loc loc;

    struct Node *items; // children
    size_t count;
    size_t capacity;
} Node;

typedef struct {
    Lexer lexer;
    Token current;
} Parser;

typedef struct {
    Node root;
} Idoc;


bool Idoc_read_entire_file(const char *path, Idoc_SB *sb)
    {
    bool result = true;

    FILE *f = fopen(path, "rb");
    size_t new_count = 0;
    long long m = 0;
    if (f == NULL)                 nob_return_defer(false);
    if (fseek(f, 0, SEEK_END) < 0) nob_return_defer(false);
#ifndef _WIN32
    m = ftell(f);
#else
    m = _telli64(_fileno(f));
#endif
    if (m < 0)                     nob_return_defer(false);
    if (fseek(f, 0, SEEK_SET) < 0) nob_return_defer(false);

    new_count = sb->count + m;
    if (new_count > sb->capacity) {
        sb->items = NOB_DECLTYPE_CAST(sb->items)NOB_REALLOC(sb->items, new_count);
        NOB_ASSERT(sb->items != NULL && "Buy more RAM lool!!");
        sb->capacity = new_count;
    }

    fread(sb->items + sb->count, m, 1, f);
    if (ferror(f)) {
        // TODO: Afaik, ferror does not set errno. So the error reporting in defer is not correct in this case.
        nob_return_defer(false);
    }
    sb->count = new_count;

defer:
    if (!result) nob_log(NOB_ERROR, "Could not read file %s: %s", path, strerror(errno));
    if (f) fclose(f);
    return result;
}

Idoc_SV sv_unquote(Idoc_SV sv) {
    if (sv.count >= 2 && sv.data[0] == '"' && sv.data[sv.count - 1] == '"') {
        sv.data++;
        sv.count -= 2;
    }
    return sv;
}

int sv_to_int(Idoc_SV sv) {
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

double sv_to_double(Idoc_SV sv) {
    double sign = 1.0f;
    double result = 0.0f;
    double fractional = 0.1f;
    size_t i = 0;
    if (sv.count > 0 && sv.data[0] == '-') { // Negative number
        sign = -1;
        i++;
    }
    for (; i < sv.count && sv.data[i] != '.'; i++) {
        result = result * 10.0f + (sv.data[i] - '0');
    }

    i++; // '.'

    for (; i < sv.count; i++) {
        result += (sv.data[i] - '0') * fractional;
        fractional *= 0.1f;
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

Lexer lexer_init(const char *file_name) {
    Idoc_SB sb = {0};
    read_entire_file(file_name, &sb);
    Idoc_SV sv = sb_to_sv(sb);
    return (Lexer){.loc.line = 1,
                   .loc.col = 1,
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
        l->loc.col += length;
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
        l->loc.col += length;
        Idoc_SV number = {.data = start, .count = length};
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
        l->loc.col += length;
        Idoc_SV number = {.data = start, .count = length};
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
        Idoc_SV variable = {
            .data = start,
            .count = length,
        };
        l->loc.col += length;
        return (Token){.type = TOKEN_VAR, .sv = variable};
    } else if (c == '=') {
        length += 1;
        sv_chop_left(&l->sv, 1);
        Idoc_SV eq = {
            .data = start,
            .count = length,
        };
        l->loc.col += length;
        return (Token){.type = TOKEN_EQUAL, .sv = eq};

    } else if (c == '\n') {
        sv_chop_left(&l->sv, 1);
        l->at_start = true;
        l->loc.line += 1;
        l->loc.col = 1;
        return (Token){.type = TOKEN_NL, .sv = sv_from_cstr("\\n")};
    } else if (c == ':') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Token){.type = TOKEN_COLON, .sv = sv_from_cstr(":")};
    } else if (c == ',') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Token){.type = TOKEN_COMMA, .sv = sv_from_cstr(",")};
    } else if (c == '(') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Token){.type = TOKEN_OPAREN, .sv = sv_from_cstr("(")};
    } else if (c == ')') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Token){.type = TOKEN_CPAREN, .sv = sv_from_cstr(")")};
    } else if (c == '"') {
        sv_chop_left(&l->sv, 1);
        length++;
        c = l->sv.data[0];
        while (c != '"') { // TODO: BUG: I think this lets you have a newline inside the string
            if (c == '\n') {error(l->file, l->loc.line, length, "Unescaped new lines are not supported");}
            sv_chop_left(&l->sv, 1);
            if (l->sv.count == 0) {
                error(l->file, l->loc.line, length, "Unbalanced '\"'.");
            }
            length++;
            c = l->sv.data[0];
        }
        sv_chop_left(&l->sv, 1);
        length++;
        Idoc_SV str = {
            .data = start,
            .count = length,
        };
        l->loc.col += length;
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
                l->loc.col = 1;
                l->loc.line += 1;
                return (Token){.type = TOKEN_NL, .sv = sv_from_cstr("\\n")};
            }
        }
    } else if (c == '.') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Token){.type = TOKEN_DOT, .sv = sv_from_cstr(".")};
    } else {
        Idoc_SV sv = {.data = &l->sv.data[0], .count = 1};
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
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


Parser parser_init(const char *file_name) {
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
        error(p->lexer.file, p->lexer.loc.line, p->lexer.loc.col,
              "Error while parsing: expected: %s  got: %s  ("SV_Fmt")",
              token_by_name(exp_tok), token_by_name(token.type), SV_Arg(token.sv));
    }
    return token;
}

Value parse_value(Parser *p, Node node, Token token) {
    node.loc = p->lexer.loc;
    switch (token.type) {
    case TOKEN_STR: {
        node.value.string = sv_unquote(token.sv);
        node.value.type = VALUE_STRING;
    } break;
    case TOKEN_INT: {
        node.value.integer = sv_to_int(token.sv);
        node.value.type = VALUE_INTEGER;
    } break;
    case TOKEN_FLOAT: {
        node.value.floating = sv_to_double(token.sv);
        node.value.type = VALUE_FLOAT;
    } break;
    case TOKEN_VAR: { // GLOBAL REFERENCE
        parser_expect(p, TOKEN_DOT);
        da_append(&node.value.ref, token.sv);
        while (true) {
            Token v = parser_expect(p, TOKEN_VAR);
            da_append(&node.value.ref, v.sv);
            if (parser_consume(p).type != TOKEN_DOT) break;
        node.value.type = VALUE_REFERENCE;
        }
    } break;
    case TOKEN_DOT: { // LOCAL REFERENCE
        da_append(&node.value.ref, token.sv);
        while (true) {
            Token v = parser_expect(p, TOKEN_VAR);
            da_append(&node.value.ref, v.sv);
            if (parser_consume(p).type != TOKEN_DOT) break;
        }
        node.value.type = VALUE_REFERENCE;
    } break;
    case TOKEN_OPAREN: {
        while (true) {
            da_append(&node.value.tuple, parse_value(p, node, parser_consume(p)));
            if (p->current.type == TOKEN_CPAREN) break;
            parser_expect(p, TOKEN_COMMA);
        }
        parser_expect(p, TOKEN_CPAREN);
        node.value.type = VALUE_TUPLE;
    } break;
    default: {
        l_error(p->lexer, "Unknown token: %s ("SV_Fmt")", token_by_name(token.type), SV_Arg(token.sv));
    } break;
    }
    return node.value;
}

Node parse_block(Parser *p, int indent_level) {
    Token t;
    Node n = {0};
    if (indent_level == 0) {
        n.key = sv_from_cstr("ROOT");
    }
    while (p->current.type != TOKEN_EOF && p->current.type != TOKEN_UNINDENT) {
        // print_token(p->current);
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
            if (parser_peek(p).type == TOKEN_EOF) {break;}
            parser_expect(p, TOKEN_UNINDENT); // SOmething is wrong here
        } else if (next.type == TOKEN_EQUAL) {
            parser_consume(p); // equal
            Token var = parser_consume(p); // whatever the assignment is
            Node sub_n = {.key = t.sv};
            sub_n.value = parse_value(p, sub_n, var);
            da_append(&n, sub_n);
        } else {
            l_error(p->lexer, "main Unknown token: %s ("SV_Fmt")", token_by_name(t.type), SV_Arg(t.sv));
        }
    }
    return n;
}

typedef struct {
    Node *node;
    bool is_valid;
} Node_Ret;

Node_Ret node_find_child(Node *parent, Idoc_SV key) {
    da_foreach(Node, it, parent) {
        if (sv_eq(it->key, key)) {
            return (Node_Ret){.node = it, .is_valid = true};
        }
    }
    return (Node_Ret){.is_valid = false};
}

void value_print(Value *value) {
    switch (value->type) {
    case VALUE_INTEGER: {
        printf("%d", value->integer);
    } break;
    case VALUE_FLOAT: {
        printf("%.2f", value->floating);
    } break;
    case VALUE_STRING: {
        printf(SV_Fmt, SV_Arg(value->string));
    } break;
    case VALUE_REFERENCE: {
        printf("Printing references is not implemented yet");
    } break;
    case VALUE_TUPLE: {
        printf("(");
        // (Type, it, da) for (Type *it = (da)->items; it < (da)->items + (da)->count; ++it)
        for (size_t i = 0; i < value->tuple.count; i++) {
            value_print(&value->tuple.items[i]);
            if (i != value->tuple.count - 1) { printf(", "); }
        }
        printf(")");
    } break;
    case VALUE_EMPTY: {
        printf("No value");
    } break;
    }
}

Idoc idoc_init(const char *file_name) {
    Parser p = parser_init(file_name);
    Node n = parse_block(&p, 0);
    return (Idoc){.root = n};
}

Value *idoc_resolve_value(Idoc *idoc, Value *value) {
    Node_Ret nr;
    while (value->type == VALUE_REFERENCE) {
        Node *current = &idoc->root;
        for (size_t i = 0; i < value->ref.count; i++) {
            nr = node_find_child(current, value->ref.items[i]);
            if (!nr.is_valid) {return NULL;}
            current = nr.node;
        }
        value = &current->value;
    }
    return value;
}

int idoc_get_int(Idoc *idoc, int def_int, ...) {
    va_list args;
    va_start(args, def_int);

    Node *current = &idoc->root;
    Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, sv_from_cstr(part));
        if (!nr.is_valid) {
            va_end(args);
            fprintf(stderr, "Warning: Could not find attribute \"%s\", using default (%d)\n", part, def_int);
            return def_int;
        }
        current = nr.node;
    }
    va_end(args);
    Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        fprintf(stderr, "Warning: Invalid reference, using default\n"); // TODO: add line num logic to this
        return def_int;
    } else if (value->type != VALUE_INTEGER) {
        fprintf(stderr, "Warning: Invalid type, using default\n"); // TODO: add line num logic to this
        return def_int;
    }
    return value->integer;
}

double idoc_get_double(Idoc *idoc, double def_double, ...) {
    va_list args;
    va_start(args, def_double);

    Node *current = &idoc->root;
    Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, sv_from_cstr(part));
        if (!nr.is_valid) {
            fprintf(stderr, "Warning: Could not find attribute \"%s\", using default (%f)\n", part, def_double);
            va_end(args);
            return def_double;
        }
        current = nr.node;
    }
    va_end(args);
    Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        fprintf(stderr, "Warning: Invalid reference, using default\n"); // TODO: add line num logic to this
        return def_double;
    } else if (value->type != VALUE_FLOAT) {
        fprintf(stderr, "Warning: Invalid type, using default\n"); // TODO: add line num logic to this
        return def_double;
    }
    return value->floating;
}

const char *idoc_get_cstr(Idoc *idoc, const char *def_str, ...) {
    va_list args;
    va_start(args, def_str);

    Node *current = &idoc->root;
    Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, sv_from_cstr(part));
        if (!nr.is_valid) {
            fprintf(stderr, "Warning: Could not find attribute \"%s\", using default (%s)\n", part, def_str);
            va_end(args);
            return def_str;
        }
        current = nr.node;
    }
    va_end(args);
    Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        fprintf(stderr, "Warning: Invalid reference, using default (%s)\n", def_str); // TODO: add line num logic to this
        return def_str;
    } else if (value->type != VALUE_STRING) {
        fprintf(stderr, "Warning: Invalid type, using default (%s)\n", def_str); // TODO: add line num logic to this
        return def_str;
    }
    return nob_temp_sv_to_cstr(value->string);
}

bool idoc_get_tuple_int(Idoc *idoc, int *out, size_t capacity, ...) {
    va_list args;
    va_start(args, capacity);

    Node *current = &idoc->root;
    Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, sv_from_cstr(part));
        if (!nr.is_valid) {
            fprintf(stderr, "Warning: Could not find attribute \"%s\"\n", part);
            va_end(args);
            return false;
        }
        current = nr.node;
    }
    va_end(args);
    Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        fprintf(stderr, "Warning: Invalid reference, using default\n"); // TODO: add line num logic to this
        return false;
    } else if (value->type != VALUE_TUPLE) {
        fprintf(stderr, "Warning: Invalid type, using default\n"); // TODO: add line num logic to this
        return false;
    }

    if (value->tuple.count != capacity) {return false;}

    for (size_t i = 0; i < value->tuple.count; i++) {
        if (value->tuple.items[i].type != VALUE_INTEGER) {
            return false;
        }
        out[i] = value->tuple.items[i].integer;
    }
    return true;
}

bool idoc_get_tuple_double(Idoc *idoc, double *out, size_t capacity, ...) {
    va_list args;
    va_start(args, capacity);

    Node *current = &idoc->root;
    Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, sv_from_cstr(part));
        if (!nr.is_valid) {
            fprintf(stderr, "Warning: Could not find attribute \"%s\"\n", part);
            va_end(args);
            return false;
        }
        current = nr.node;
    }
    va_end(args);
    Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        fprintf(stderr, "Warning: Invalid reference, using default\n"); // TODO: add line num logic to this
        return false;
    } else if (value->type != VALUE_TUPLE) {
        fprintf(stderr, "Warning: Invalid type, using default\n"); // TODO: add line num logic to this
        return false;
    }

    if (value->tuple.count != capacity) {return false;}

    for (size_t i = 0; i < value->tuple.count; i++) {
        if (value->tuple.items[i].type != VALUE_FLOAT) {
            return false;
        }
        out[i] = value->tuple.items[i].floating;
    }
    return true;
}

bool idoc_get_tuple_cstr(Idoc *idoc, const char **out, size_t capacity, ...) {
    va_list args;
    va_start(args, capacity);

    Node *current = &idoc->root;
    Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, sv_from_cstr(part));
        if (!nr.is_valid) {
            fprintf(stderr, "Warning: Could not find attribute \"%s\"\n", part);
            va_end(args);
            return false;
        }
        current = nr.node;
    }
    va_end(args);
    Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        fprintf(stderr, "Warning: Invalid reference, using default\n"); // TODO: add line num logic to this
        return false;
    } else if (value->type != VALUE_TUPLE) {
        fprintf(stderr, "Warning: Invalid type, using default\n"); // TODO: add line num logic to this
        return false;
    }

    if (value->tuple.count != capacity) {return false;}

    for (size_t i = 0; i < value->tuple.count; i++) {
        if (value->tuple.items[i].type != VALUE_STRING) {
            return false;
        }
        out[i] = temp_sv_to_cstr(value->tuple.items[i].string);
    }
    return true;
}
// #define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
// types: int, double, cstring
#define idoc_get(type, idoc, def, ...) idoc_get_##type(idoc, def, __VA_ARGS__, NULL)
#define idoc_get_tuple(type, idoc, out, ...) idoc_get_tuple_##type(idoc, out, ARRAY_LEN(out), __VA_ARGS__, NULL)
