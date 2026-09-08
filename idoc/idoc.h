// IDOC - Indentation DOCument.
// This single-header library is used to parse idoc files.
// To learn how to write an idoc file, read the README.

// The idea of this library is to be able to hot reload settings
// about your application, without ever raising errors even
// if something is missing.

// You start by doing
// Then check if it errored out
//     Idoc idoc = idoc_init("filename");
//     if (!idoc.is_valid) exit(1);
// Then use the various helper functions to get specific values from the document.
// Those functions are explained below.

#ifndef IDOC_H
#define IDOC_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

// This is the main structure you work with
typedef struct Idoc Idoc;

// You start by parsing the file using idoc_init
Idoc idoc_init(const char *file_name);
// Make sure to check if its valid!! The file can fail to read.
// Example:
#if 0
    Idoc idoc = idoc_init("filename");
    if (!idoc.is_valid) exit(1); // or do whatever you want here
#endif

// Suppose your .idoc file looks like this:
//
// Program:
//   Name = "ProgramName"
//
// To get the program name, you use the corresponding function
// Example:
#if 0
    char *name = idoc_get_cstr(&idoc, "default_name", "Program", "Name", NULL);
#endif
// Notice how we get the string by tracing out the nested path to what we want,
// and terminate it by NULL.
// If theres any error retrieving (a.k.a. doesnt exist or is invalid) the
// default name will be used.
//
// You can use the following functions to get each type of field:
int          idoc_get_int(Idoc *idoc, int def_int, ...);
double       idoc_get_double(Idoc *idoc, double def_double, ...);
// The user must manually free the cstring, IT IS NOT FREED BY idoc_free
char        *idoc_get_cstr(Idoc *idoc, const char *def_str, ...);

// For tuples, you must define an array of the same size as the tuple,
// and pass it.
// These functions will return true if succeeded or false if not,
// as not to stop the program even if something goes wrong.
// It will return false if the type is incorrect, or if the size
// is incorrect.

// Suppose our new .idoc file looks like this:
//
// Program:
//   Name = "ProgramName"
//   Palette:
//     Color = (255, 255, 0)
//
// Then, to get this color tuple, we do the following
// Example:
#if 0
    int color[3];
    idoc_get_tuple_int(&idoc, color, 3, "Program", "Palette", "Color", NULL);
#endif
// Note that we must pass the capacity of the array using this function,
// and again, pass the NULL as the last value.
// We define an easier more straightforward way of getting the same value
// using macros below.
// Here are the tuple functions:
bool         idoc_get_tuple_int(Idoc *idoc, int *out, size_t capacity, ...);
bool         idoc_get_tuple_double(Idoc *idoc, double *out, size_t capacity, ...);
bool         idoc_get_tuple_cstr(Idoc *idoc, char **out, size_t capacity, ...);

// Finally, after getting all you want from the file, you can use idoc_free to
// free the whole object. Don't forget that idoc_get_cstr doesnt get freed here!!
void idoc_free(Idoc *idoc);

#define IDOC_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
// For the convinience of the user, there are macros provided using the syntax
// idoc_get(&idoc, TYPE, DEFAULT, ...);
// So, using the same .idoc file defined above:
//
// Program:
//   Name = "ProgramName"
//   Palette:
//     Color = (255, 255, 0)
//
// We can get the same information as before as following:
// Example:
#if 0
    char *name = idoc_get(&idoc, cstr, "default_name", "Program", "Name")
    int color[3];
    idoc_get_tuple_int(&idoc, color, "Program", "Palette", "Color");
#endif
// Using macros, you dont need to pass the last NULL, nor pass the size of the
// array.
// Here are all the types idoc supports: int, double, cstr
#define idoc_get(idoc, type, def, ...) idoc_get_##type(idoc, def, __VA_ARGS__, NULL)
#define idoc_get_tuple(idoc, type, out, ...) idoc_get_tuple_##type(idoc, out, IDOC_ARRAY_LEN(out), __VA_ARGS__, NULL)


// In case of an incorrect type, it automatically prints to stderr a warning.
// If you dont want this behavior, define the following before including this
// to supress all warnings:
// #define IDOC_NO_WARNINGS
// This does not disable out of memory or other errors!!


#ifdef IDOC_IMPLEMENTATION

#ifndef IDOC_NO_WARNINGS
#define IDOC_WARN(...) fprintf(stderr, __VA_ARGS__)
#else
#define IDOC_WARN(...)
#endif // IDOC_NO_WARNINGS

#define COMMENT_CHAR '#'

typedef enum {
    WS_UNSET = -1,
    WS_NO_INDENT,
    WS_TAB = '\t',
    WS_SPACE = ' ',
} Idoc_whitespace_type;

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
} Idoc_Token_Type;

const char *token_by_name(Idoc_Token_Type type) {
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

#define SV_FMT "%.*s"
#define SV_ARG(sv) (int)(sv).count, (sv).data

#define da_append(da, value)                                                   \
    do {                                                                       \
        if ((da)->count >= (da)->capacity) {                                   \
            size_t new_capacity = (da)->capacity ? (da)->capacity * 2 : 8;     \
            void *new_items =                                                  \
                realloc((da)->items, new_capacity * sizeof(*(da)->items));     \
            if (!new_items) {                                                  \
                fprintf(stderr, "No memory!!!\n");                             \
                exit(1);                                                       \
            }                                                                  \
            (da)->items = new_items;                                           \
            (da)->capacity = new_capacity;                                     \
        }                                                                      \
        (da)->items[(da)->count++] = (value);                                  \
    } while (0)




typedef struct {
    Idoc_Token_Type type;
    Idoc_SV sv;
} Idoc_Token;

typedef struct {
    Idoc_whitespace_type type;
    size_t count;
} Idoc_Indent;

typedef struct {
    int line;
    int col;
} Idoc_Loc;

typedef struct {
    Idoc_Loc loc;
    Idoc_SV sv;
    const char *file;
    bool at_start;
    Idoc_Indent exp_ind;

    Idoc_Indent last_ind;
} Lexer;


typedef enum {
    VALUE_EMPTY,
    VALUE_STRING,
    VALUE_INTEGER,
    VALUE_FLOAT,
    VALUE_TUPLE,
    VALUE_REFERENCE,
} Idoc_Value_Type;

typedef enum {
    NODE_BLOCK,
    NODE_STRING,
    NODE_INTEGER,
    NODE_REFERENCE,
} Idoc_Node_Type;

typedef struct Idoc_Value Idoc_Value;

struct Idoc_Value {
    Idoc_Value_Type type;
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
            Idoc_Value *items;
            size_t count;
            size_t capacity;
        } tuple;
    };
};

typedef struct Idoc_Node {
    Idoc_SV key;
    Idoc_Value value;
    Idoc_Loc loc;

    struct Idoc_Node *items; // children
    size_t count;
    size_t capacity;
} Idoc_Node;

typedef struct {
    Lexer lexer;
    Idoc_Token current;
} Idoc_Parser;

struct Idoc {
    Idoc_Node root;
    Idoc_SB __file_content;
    bool is_valid;
};

bool idoc_read_entire_file(const char *path, Idoc_SB *sb) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s: %s\n",
                path, strerror(errno));
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return false;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    size_t new_count = sb->count + (size_t)size;

    if (new_count > sb->capacity) {
        size_t capacity = sb->capacity ? sb->capacity : 1024;

        while (capacity < new_count) {
            capacity *= 2;
        }

        char *new_items = realloc(sb->items, capacity);

        if (!new_items) {
            fprintf(stderr, "Out of memory reading %s\n", path);
            fclose(f);
            return false;
        }

        sb->items = new_items;
        sb->capacity = capacity;
    }

    size_t read = fread(
        sb->items + sb->count,
        1,
        (size_t)size,
        f
    );

    if (read != (size_t)size) {
        fprintf(stderr, "Could not read %s\n", path);
        fclose(f);
        return false;
    }

    sb->count = new_count;

    fclose(f);
    return true;
}

Idoc_SV cstr_to_sv(const char *str) {
    return (Idoc_SV){
        .data = str,
        .count = strlen(str)
    };
}

char *cstr_copy(const char *str) {
    size_t len = strlen(str);

    char *copy = malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "No memory!!!\n");
        exit(1);
    }

    memcpy(copy, str, len + 1);

    return copy;
}

// The user owns the memory so you can freely use the string after idoc_free.
// Make sure to free it with free(str)!
char *sv_to_cstr(Idoc_SV sv) {
    char *string = malloc(sv.count + 1);
    if (string == NULL) {
        fprintf(stderr, "No memory!!!\n");
        exit(1);
    }
    memcpy(string, sv.data, sv.count);
    string[sv.count] = '\0';
    return string;
}

bool sv_eq(Idoc_SV sv1, Idoc_SV sv2) {
    if (sv1.count != sv2.count) return false;
    for (size_t i = 0; i < sv1.count; i++) {
        unsigned char c1 = (unsigned char)sv1.data[i];
        unsigned char c2 = (unsigned char)sv2.data[i];
        if (tolower(c1) != tolower(c2)) return false;
    }
    return true;
}

Idoc_SV sv_chop_left(Idoc_SV *sv, int count) {
    Idoc_SV chopped = {
        .data = sv->data,
        .count = count
    };
    sv->data  += count;
    sv->count -= count;
    return chopped;
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

void error(const char *file_name, int line, int col, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s:%d:%d: ", file_name, line, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

Lexer lexer_init(Idoc_SV file, const char* file_name) {
    return (Lexer){.loc.line = 1,
                   .loc.col = 1,
                   .sv = file,
                   .file = file_name,
                   .exp_ind = (Idoc_Indent){WS_UNSET, 0},
                   .at_start = true};
}

char lexer_peek(Lexer *l) {
    if (l->sv.count == 0) {
        return '\0';
    }
    return l->sv.data[0];
}

bool lexer_check_whitespace(Lexer *l, Idoc_whitespace_type ws, size_t length) {
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
        l->exp_ind = (Idoc_Indent){.type = ws, .count = length};
        return true;
    }
}

Idoc_Token lexer_next_token(Lexer *l) {
    if (l->sv.count == 0) {
        return (Idoc_Token){.type = TOKEN_EOF, .sv = cstr_to_sv("EOF")};
    }
    size_t length = 0;
    char c = lexer_peek(l);
    Idoc_whitespace_type ws;
    while (true) { // Whitespace handling
        if (c == ' ') {
            ws = WS_SPACE;
            sv_chop_left(&l->sv, 1);
            length++;
            c = lexer_peek(l);
        } else if (c == '\t') {
            ws = WS_TAB;
            sv_chop_left(&l->sv, 1);
            length++;
            c = lexer_peek(l);
        } else if (c == '\r') { // windows carriage return..
            sv_chop_left(&l->sv, 1);
            c = lexer_peek(l);
        } else
            break;
    }
    // If there is a whitespace, handle it:
    if (length > 0) {
        l->loc.col += length;
        if (lexer_check_whitespace(l, ws, length)) {
            if (l->last_ind.count < length) {
                l->last_ind = (Idoc_Indent){.type = ws, .count = length};
                return (Idoc_Token){.type = TOKEN_INDENT, .sv = cstr_to_sv("->")};
            } else if (l->last_ind.count > length) {
                l->last_ind = (Idoc_Indent){.type = ws, .count = length};
                return (Idoc_Token){.type = TOKEN_UNINDENT,
                               .sv = cstr_to_sv("<-")};
            }
        }
    }
    length = 0;
    const char *start = l->sv.data;
    if (isdigit(c)) {
        Idoc_Token_Type tt = TOKEN_INT;
        while (true) {
            sv_chop_left(&l->sv, 1);
            length++;
            c = lexer_peek(l);
            if (c == '.')
                tt = TOKEN_FLOAT;
            else if (!isdigit(c))
                break;
        }
        l->loc.col += length;
        Idoc_SV number = {.data = start, .count = length};
        return (Idoc_Token){.type = tt, .sv = number};
    } else if (c == '-' && isdigit(l->sv.data[1])) { // The next must be a number
        Idoc_Token_Type tt = TOKEN_INT;
        while (true) {
            sv_chop_left(&l->sv, 1);
            length++;
            c = lexer_peek(l);
            if (c == '.')
                tt = TOKEN_FLOAT;
            else if (!isdigit(c))
                break;
        }
        l->loc.col += length;
        Idoc_SV number = {.data = start, .count = length};
        return (Idoc_Token){.type = tt, .sv = number};
    }
    if (isalpha(c)) {
        while (true) {
            sv_chop_left(&l->sv, 1);
            length++;
            c = lexer_peek(l);
            if (!isalpha(c) && !isdigit(c))
                break;
        }
        Idoc_SV variable = {
            .data = start,
            .count = length,
        };
        l->loc.col += length;
        return (Idoc_Token){.type = TOKEN_VAR, .sv = variable};
    } else if (c == '=') {
        length += 1;
        sv_chop_left(&l->sv, 1);
        Idoc_SV eq = {
            .data = start,
            .count = length,
        };
        l->loc.col += length;
        return (Idoc_Token){.type = TOKEN_EQUAL, .sv = eq};

    } else if (c == '\n') {
        sv_chop_left(&l->sv, 1);
        l->at_start = true;
        l->loc.line += 1;
        l->loc.col = 1;
        return (Idoc_Token){.type = TOKEN_NL, .sv = cstr_to_sv("\\n")};
    } else if (c == ':') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Idoc_Token){.type = TOKEN_COLON, .sv = cstr_to_sv(":")};
    } else if (c == ',') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Idoc_Token){.type = TOKEN_COMMA, .sv = cstr_to_sv(",")};
    } else if (c == '(') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Idoc_Token){.type = TOKEN_OPAREN, .sv = cstr_to_sv("(")};
    } else if (c == ')') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Idoc_Token){.type = TOKEN_CPAREN, .sv = cstr_to_sv(")")};
    } else if (c == '"') {
        sv_chop_left(&l->sv, 1);
        length++;
        c = lexer_peek(l);
        while (c != '"') { // TODO: BUG: I think this lets you have a newline inside the string
            if (c == '\n') {error(l->file, l->loc.line, length, "Unescaped new lines are not supported");}
            sv_chop_left(&l->sv, 1);
            if (l->sv.count == 0) {
                error(l->file, l->loc.line, length, "Unbalanced '\"'.");
            }
            length++;
            c = lexer_peek(l);
        }
        sv_chop_left(&l->sv, 1);
        length++;
        Idoc_SV str = {
            .data = start,
            .count = length,
        };
        l->loc.col += length;
        return (Idoc_Token){.type = TOKEN_STR, .sv = str};

    } else if (c == COMMENT_CHAR) {
        while (true) {
            if (l->sv.count == 0) {
                return (Idoc_Token){.type = TOKEN_EOF, .sv = cstr_to_sv("EOF")};
            }
            sv_chop_left(&l->sv, 1);
            c = lexer_peek(l);
            if (c == '\n') {
                sv_chop_left(&l->sv, 1);
                l->at_start = true;
                l->loc.col = 1;
                l->loc.line += 1;
                return (Idoc_Token){.type = TOKEN_NL, .sv = cstr_to_sv("\\n")};
            }
        }
    } else if (c == '.') {
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Idoc_Token){.type = TOKEN_DOT, .sv = cstr_to_sv(".")};
    } else {
        Idoc_SV sv = {.data = l->sv.data, .count = 1};
        sv_chop_left(&l->sv, 1);
        l->loc.col += 1;
        return (Idoc_Token){.type = TOKEN_UNDEFINED, .sv = sv};
    }
}

void print_token(Idoc_Token t) {
    printf(SV_FMT " => %s\n", SV_ARG(t.sv), token_by_name(t.type));
}

void dump_tokens(Lexer *l) {
    Idoc_Token t;
    do {
        t = lexer_next_token(l);
        print_token(t);
    } while (t.type != TOKEN_EOF);
}


Idoc_Parser parser_init(Idoc_SV file, const char* file_name) {
    Idoc_Parser parser = {
        .lexer = lexer_init(file, file_name),
    };
    parser.current = lexer_next_token(&parser.lexer);
    return parser;
}

Idoc_Token parser_peek(Idoc_Parser *p) { return p->current; }

Idoc_Token parser_consume(Idoc_Parser *p) {
    Idoc_Token t = p->current;
    p->current = lexer_next_token(&p->lexer);
    return t;
}

Idoc_Token parser_expect(Idoc_Parser *p, Idoc_Token_Type exp_tok) {
    Idoc_Token token = parser_consume(p);
    if (token.type != exp_tok) {
        error(p->lexer.file, p->lexer.loc.line, p->lexer.loc.col,
              "Error while parsing: expected: %s  got: %s  ("SV_FMT")",
              token_by_name(exp_tok), token_by_name(token.type), SV_ARG(token.sv));
    }
    return token;
}

Idoc_Value parse_value(Idoc_Parser *p, Idoc_Node node, Idoc_Token token) {
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
            Idoc_Token v = parser_expect(p, TOKEN_VAR);
            da_append(&node.value.ref, v.sv);
            if (parser_consume(p).type != TOKEN_DOT) break;
        node.value.type = VALUE_REFERENCE;
        }
    } break;
    case TOKEN_DOT: { // LOCAL REFERENCE
        da_append(&node.value.ref, token.sv);
        while (true) {
            Idoc_Token v = parser_expect(p, TOKEN_VAR);
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
        error(p->lexer.file, p->lexer.loc.line, p->lexer.loc.col,
              "Unknown token: %s ("SV_FMT")", token_by_name(token.type), SV_ARG(token.sv));
    } break;
    }
    return node.value;
}

Idoc_Node parse_block(Idoc_Parser *p, int indent_level) {
    Idoc_Token t;
    Idoc_Node n = {0};
    if (indent_level == 0) {
        n.key = cstr_to_sv("ROOT");
    }
    while (p->current.type != TOKEN_EOF && p->current.type != TOKEN_UNINDENT) {
        // print_token(p->current);
        if (parser_peek(p).type == TOKEN_NL) {
            parser_consume(p);
            continue;
        }
        t = parser_expect(p, TOKEN_VAR);
        Idoc_Token next = parser_peek(p);
        if (next.type == TOKEN_COLON) {
            parser_consume(p); // colon
            parser_expect(p, TOKEN_NL);
            parser_expect(p, TOKEN_INDENT);
            Idoc_Node sub_n = parse_block(p, indent_level + 1);
            sub_n.key = t.sv;
            da_append(&n, sub_n);
            if (parser_peek(p).type == TOKEN_EOF) {break;}
            parser_expect(p, TOKEN_UNINDENT); // SOmething is wrong here
        } else if (next.type == TOKEN_EQUAL) {
            parser_consume(p); // equal
            Idoc_Token var = parser_consume(p); // whatever the assignment is
            Idoc_Node sub_n = {.key = t.sv};
            sub_n.value = parse_value(p, sub_n, var);
            da_append(&n, sub_n);
        } else {
            error(p->lexer.file, p->lexer.loc.line, p->lexer.loc.col, "Unknown token: %s ("SV_FMT")", token_by_name(t.type), SV_ARG(t.sv));
        }
    }
    return n;
}

typedef struct {
    Idoc_Node *node;
    bool is_valid;
} Idoc_Node_Ret;

Idoc_Node_Ret node_find_child(Idoc_Node *parent, Idoc_SV key) {
    // da_foreach(Idoc_Node, it, parent) {
    for (size_t i = 0; i < parent->count; i++) {
        if (sv_eq(parent->items[i].key, key)) {
            return (Idoc_Node_Ret){.node = &parent->items[i], .is_valid = true};
        }
    }
    return (Idoc_Node_Ret){.is_valid = false};
}


void value_print(Idoc_Value *value) {
    switch (value->type) {
    case VALUE_INTEGER: {
        printf("%d", value->integer);
    } break;
    case VALUE_FLOAT: {
        printf("%.2f", value->floating);
    } break;
    case VALUE_STRING: {
        printf(SV_FMT, SV_ARG(value->string));
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
    Idoc ret = {0};
    Idoc_SB sb = {0};
    if (!idoc_read_entire_file(file_name, &sb)) {
        ret.is_valid = false;
        return ret
    }
    Idoc_SV sv = {
        .data = sb.items,
        .count = sb.count
    };
    Idoc_Parser p = parser_init(sv, file_name);
    Idoc_Node n = parse_block(&p, 0);
    ret.root = n;
    ret.__file_content = sb;
    ret.is_valid = true;
    return ret;
}

Idoc_Value *idoc_resolve_value(Idoc *idoc, Idoc_Value *value) {
    Idoc_Node_Ret nr;
    while (value->type == VALUE_REFERENCE) {
        Idoc_Node *current = &idoc->root;
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

    Idoc_Node *current = &idoc->root;
    Idoc_Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, cstr_to_sv(part));
        if (!nr.is_valid) {
            va_end(args);
            IDOC_WARN("Warning: Could not find attribute \"%s\", using default (%d)\n", part, def_int);
            return def_int;
        }
        current = nr.node;
    }
    va_end(args);
    Idoc_Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        IDOC_WARN("Warning: Invalid reference, using default\n"); // TODO: add line num logic to this
        return def_int;
    } else if (value->type != VALUE_INTEGER) {
        IDOC_WARN("Warning: Invalid type, using default (%d)\n", def_int); // TODO: add line num logic to this
        return def_int;
    }
    return value->integer;
}

double idoc_get_double(Idoc *idoc, double def_double, ...) {
    va_list args;
    va_start(args, def_double);

    Idoc_Node *current = &idoc->root;
    Idoc_Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, cstr_to_sv(part));
        if (!nr.is_valid) {
            IDOC_WARN("Warning: Could not find attribute \"%s\", using default (%f)\n", part, def_double);
            va_end(args);
            return def_double;
        }
        current = nr.node;
    }
    va_end(args);
    Idoc_Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        IDOC_WARN("Warning: Invalid reference, using default\n"); // TODO: add line num logic to this
        return def_double;
    } else if (value->type != VALUE_FLOAT) {
        IDOC_WARN("Warning: Invalid type, using default (%f)\n", def_double); // TODO: add line num logic to this
        return def_double;
    }
    return value->floating;
}

char *idoc_get_cstr(Idoc *idoc, const char *def_str, ...) {
    va_list args;
    va_start(args, def_str);

    Idoc_Node *current = &idoc->root;
    Idoc_Node_Ret nr;
    const char *part;
    char *cp_def_str = cstr_copy(def_str);
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, cstr_to_sv(part));
        if (!nr.is_valid) {
            IDOC_WARN("Warning: Could not find attribute \"%s\", using default (%s)\n", part, cp_def_str);
            va_end(args);
            return cp_def_str;
        }
        current = nr.node;
    }
    va_end(args);
    Idoc_Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        IDOC_WARN("Warning: Invalid reference, using default (%s)\n", cp_def_str); // TODO: add line num logic to this
        return cp_def_str;
    } else if (value->type != VALUE_STRING) {
        IDOC_WARN("Warning: Invalid type, using default (%s)\n", cp_def_str); // TODO: add line num logic to this
        return cp_def_str;
    }
    free(cp_def_str);
    return sv_to_cstr(value->string);
}

bool idoc_get_tuple_int(Idoc *idoc, int *out, size_t capacity, ...) {
    va_list args;
    va_start(args, capacity);

    Idoc_Node *current = &idoc->root;
    Idoc_Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, cstr_to_sv(part));
        if (!nr.is_valid) {
            IDOC_WARN("Warning: Could not find attribute \"%s\"\n", part);
            va_end(args);
            return false;
        }
        current = nr.node;
    }
    va_end(args);
    Idoc_Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        IDOC_WARN("Warning: Invalid reference\n"); // TODO: add line num logic to this
        return false;
    } else if (value->type != VALUE_TUPLE) {
        IDOC_WARN("Warning: Invalid type\n"); // TODO: add line num logic to this
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

    Idoc_Node *current = &idoc->root;
    Idoc_Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, cstr_to_sv(part));
        if (!nr.is_valid) {
            IDOC_WARN("Warning: Could not find attribute \"%s\"\n", part);
            va_end(args);
            return false;
        }
        current = nr.node;
    }
    va_end(args);
    Idoc_Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        IDOC_WARN("Warning: Invalid reference\n"); // TODO: add line num logic to this
        return false;
    } else if (value->type != VALUE_TUPLE) {
        IDOC_WARN("Warning: Invalid type\n"); // TODO: add line num logic to this
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

bool idoc_get_tuple_cstr(Idoc *idoc, char **out, size_t capacity, ...) {
    va_list args;
    va_start(args, capacity);

    Idoc_Node *current = &idoc->root;
    Idoc_Node_Ret nr;
    const char *part;
    while (true) {
        part = va_arg(args, const char *);
        if (part == NULL) {
            break; // reached the end
        }
        nr = node_find_child(current, cstr_to_sv(part));
        if (!nr.is_valid) {
            IDOC_WARN("Warning: Could not find attribute \"%s\"\n", part);
            va_end(args);
            return false;
        }
        current = nr.node;
    }
    va_end(args);
    Idoc_Value *value = idoc_resolve_value(idoc, &current->value);
    if (value == NULL) {
        IDOC_WARN("Warning: Invalid reference\n"); // TODO: add line num logic to this
        return false;
    } else if (value->type != VALUE_TUPLE) {
        IDOC_WARN("Warning: Invalid type\n"); // TODO: add line num logic to this
        return false;
    }

    if (value->tuple.count != capacity) {return false;}

    for (size_t i = 0; i < value->tuple.count; i++) {
        if (value->tuple.items[i].type != VALUE_STRING) {
            return false;
        }
        out[i] = sv_to_cstr(value->tuple.items[i].string);
    }
    return true;
}

void value_free(Idoc_Value *value) {
    switch (value->type) {
    case VALUE_REFERENCE: {
        free(value->ref.items);
    } break;
    case VALUE_TUPLE: {
        for (size_t i = 0; i < value->tuple.count; i++) {
            value_free(&value->tuple.items[i]);
        }
        free(value->tuple.items);
    } break;
    default: break;
    }
}

void node_free(Idoc_Node *node) {
    value_free(&node->value);
    for (size_t i = 0; i < node->count; i++) {
        node_free(&node->items[i]);
    }
    free(node->items);
}

void idoc_free(Idoc *idoc) {
    node_free(&idoc->root);
    free(idoc->__file_content.items);
}


#endif // IDOC_IMPLEMENTATION
#endif // IDOC_H
