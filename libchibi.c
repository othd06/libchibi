
#include "chibicc.h"

#include <unistd.h>
#include <sys/wait.h>



typedef enum {
    BTY_VOID,
    BTY_BOOL,
    BTY_CHAR,
    BTY_SHORT,
    BTY_INT,
    BTY_LONG,
    BTY_FLOAT,
    BTY_DOUBLE,
    BTY_LDOUBLE,
} BaseType;

Type* create_base_type(BaseType type) {
    Type* btype = calloc(1, sizeof(Type));
    if (type == BTY_VOID) {
        btype->kind = TY_VOID;
        btype->size = 0;
    } else if (type == BTY_BOOL) {
        btype->kind = TY_BOOL;
        btype->size = 1;
        btype->align = 1;
    } else if (type == BTY_CHAR) {
        btype->kind = TY_CHAR;
        btype->size = 1;
        btype->align = 1;
    } else if (type == BTY_SHORT) {
        btype->kind = TY_SHORT;
        btype->size = 2;
        btype->align = 2;
    } else if (type == BTY_INT) {
        btype->kind = TY_INT;
        btype->size = 4;
        btype->align = 4;
    } else if (type == BTY_LONG) {
        btype->kind = TY_LONG;
        btype->size = 8;
        btype->align = 8;
    } else if (type == BTY_FLOAT) {
        btype->kind = TY_FLOAT;
        btype->size = 4;
        btype->align = 4;
    } else if (type == BTY_DOUBLE) {
        btype->kind = TY_DOUBLE;
        btype->size = 8;
        btype->align = 8;
    } else if (type == BTY_LDOUBLE) {
        btype->kind = TY_LDOUBLE;
        btype->size = 16;
        btype->align = 16;
    }
    btype->next = NULL;
    return btype;
}

Type* create_enum_type() {
    Type* enumty = calloc(1, sizeof(Type));
    enumty->kind = TY_ENUM;
    enumty->size = 4;
    enumty->align = 4;
    enumty->members = //TODO: figure out something sensible to put here
    enumty->next = NULL;
    return enumty;
}

Type* create_ptr_type(Type* type) {
    Type* ptrty = calloc(1, sizeof(Type));
    ptrty->kind = TY_PTR;
    ptrty->size = 8;
    ptrty->align = 8;
    ptrty->base = type;
    ptrty->next = NULL;
    return ptrty;
}

Type* create_function_type(Type* return_ty, Type** params, bool is_variadic = false) {
    Type* funtype = calloc(1, sizeof(Type));
    funtype->kind = TY_FUNC;
    funtype->return_ty = return_ty;
    funtype->params = *params;
    funtype->is_variadic = is_variadic;
    funtype->next = NULL;
    return funtype;
}

Type* create_array_type(Type* type, int len) {
    Type* arraytype = calloc(1, sizeof(Type));
    arraytype->kind = TY_ARRAY;
    arraytype->size = type->size*len;
    arraytype->align = type->align;
    arraytype->array_len = len;
    arraytype->base = type;
    arraytype->next = NULL;
    return arraytype;
}

Type* create_variable_length_array_type(Type* type) {
    Type* vlatype = calloc(1, sizeof(Type));
    vlatype->kind = TY_VLA;
    vlatype->align = type->align;
    vlatype->base = type;
    vlatype->vla_length = //TODO: figure out something sensible here
    vlatype->vla_size = //TODO: figure out something sinsible here
    vlatype->next = NULL;
    return vlatype;
}

Type* create_struct_type(is_packed = false, is_flexible = false) {
    Type* structtype = calloc(1, sizeof(Type));
    structtype->kind = TY_STRUCT;
    structtype->size = //TODO: figure out something sensible here
    structtype->align = //TODO: figure out something sensible here
    structtype->members = //TODO: figure out something sensible here
    structtype->is_packed = is_packed;
    structtype->is_flexible = is_flexible;
    structtype->next = NULL;
    return structtype;
}

Type* create_union_type(is_packed = false) {
    Type* uniontype = calloc(1, sizeof(Type));
    uniontype->kind = TY_UNION;
    uniontype->size = //TODO: figure out something sensible here
    uniontype->align = //TODO: figure out something sensible here
    uniontype->members = //TODO: figure out something sensible here
    uniontype->is_packed = is_packed;
    uniontype->next = NULL;
    return uniontype;
}

void append_type(Type** list, Type* type) {
    if (*list == NULL) {*list = type; return;}
    Type* current = *list;
    while (current->next != NULL) current = current->next;
    current->next = type;
}

Obj* create_function_definition(char* name, Type* ret_type, Type** params, bool is_static = false, bool is_local = false) {
    Obj* definition = calloc(1, sizeof(Obj));
    definition->next = NULL;
    definition->name = name;
    definition->ty = ret_type;
    definition->is_function = true;
    definition->is_definition = true;
    definition->is_static = is_static;
    definition->params = *params;
    definition->locals = //TODO: figure out something sensible here
    definition->body = //TODO: figure out something sensible here
    definition->is_local = is_local;
    return definition;
}

Obj* create_function_declaration(char* name, *Type ret_type, bool is_static = false) {
    Obj* declaration = calloc(1, sizeof(Obj));
    declaration->next = NULL;
    declaration->name = name;
    declaration->ty = ret_type;
    declaration->is_function = true;
    declaration->is_definition = false;
    declaration->is_static = is_static;
    return declaration;
}

Obj* create_global_variable_definition(char* name, Type* type, bool is_static = false, bool is_tls = false, bool is_tentative = false) {
    Obj* definition = calloc(1, sizeof(Obj));
    definition->next = NULL;
    definition->name = name;
    definition->is_function = false;
    definition->ty = type;
    definition->is_static = is_static;
    definition->is_tls = is_tls;
    definition->is_tentative = is_tentative;
    definition->init_data = //TODO: figure out something sensible here
    definition->rel = //TODO: figure out something sensible here
    definition->is_local = //TODO: figure out something sensible here
    return definition;
}

Obj* create_global_variable_declaration(char* name, Type* type) {
    Obj* declaration = calloc(1, sizeof(Obj));
    declaration->next = NULL;
    declaration->name = name;
    declaration->is_function = false;
    declaration->ty = type;
    declaration->is_static = //TODO: figure out something sensible here
    return declaration;
}

void append_object(Obj** list, Obj* object) {
    if (*list == NULL) {*list = object; return;}
    Obj* current = *list;
    while (current->next != NULL) current = current->next;
    current->next = object;
}

bool bubble_greater(Obj* a, Obj* b) {
    int a_sect = 0;
    int b_sect = 0;
    {
        if (a->is_function) {
            //a_sect: 6-7
            if (a->is_definition) a_sect = 7; else a_sect = 6;
        } else {
            //a_sect: 1-5
            if (a->is_tls) {
                //a_sect: 1-2
                if (a->init_data) a_sect = 1; else a_sect = 2;
            } else if (a->is_tentative && !a->is_static) a_sect = 5;
            else {
                //a_sect: 3-4
                if (a->init_data) a_sect = 3; else a_sect = 4;
            }
        }
    }
    {
        if (b->is_function) {
            //b_sect: 6-7
            if (b->is_definition) b_sect = 7; else b_sect = 6;
        } else {
            //b_sect: 1-5
            if (b->is_tls) {
                //b_sect: 1-2
                if (b->init_data) b_sect = 1; else b_sect = 2;
            } else if (b->is_tentative && !b->is_static) b_sect = 5;
            else {
                //b_sect: 3-4
                if (b->init_data) b_sect = 3; else b_sect = 4;
            }
        }
    }
    return a_sect>b_sect;
}
bool bubble_pass(Obj** list) {
    Obj* current = *list;
    Obj* last = NULL;
    bool complete = true;
    while (current->next != NULL){
        if (bubble_greater(current, current->next)) {
            complete = false;
            if (last != NULL) {
                last->next = current->next;
                current->next = current->next->next;
                last->next->next = current;
                last = last->next;
            }
            else {
                Obj* temp = current->next->next;
                *list = current->next;
                current->next->next = current;
                current->next = temp;
                last = *list;
            }
        }
        else {
            last = current;
            current = current->next;
        }
    }
    return complete;
}
void resection_objects(Obj** list) {
    while (!bubble_pass(list)) {}
}


bool opt_fpic = false;
bool opt_fcommon = false;
DebugFile *debug_files = NULL;
int debug_file_count = 0;

static void register_debug_files(DebugFile *files, int count) {debug_files = files; debug_file_count = count;}
static void set_fpic(bool enabled) {opt_fpic = enabled;}
static void set_fcommon(bool enabled) {opt_fcommon = enabled;}

static FILE *open_file(char *path) {
    if (!path || strcmp(path, "-") == 0)
        return stdout;

    FILE *out = fopen(path, "w");
    if (!out)
        error("cannot open output file: %s: %s", path, strerror(errno));
    return out;
}

static void cProg(Obj *prog, char *output_file) {
    // Open a temporary output buffer.
    char *buf;
    size_t buflen;
    FILE *output_buf = open_memstream(&buf, &buflen);

    // Traverse the AST to emit assembly.
    codegen(prog, output_buf);
    fclose(output_buf);

    // Write the asembly text to a file.
    FILE *out = open_file(output_file);
    fwrite(buf, buflen, 1, out);
    fclose(out);
}

static void aProg(Obj *prog, char *output_file) {
    // 1. Create a temporary file for the assembly output.
    char asm_template[] = "/tmp/libchibi-XXXXXX.s";

    int fd = mkstemp(asm_template);
    if (fd < 0)
        error("mkstemp failed: %s", strerror(errno));

    FILE *tmp = fdopen(fd, "w");
    if (!tmp)
        error("fdopen failed: %s", strerror(errno));

    // 2. Generate assembly into the temporary file.
    fclose(tmp); 
    cProg(prog, asm_template);

    // 3. Fork and exec the assembler.
    pid_t pid = fork();
    if (pid < 0)
        error("fork failed: %s", strerror(errno));

    if (pid == 0) {
        // Child: exec 'as'
        execlp("as", "as", "-o", output_file, asm_template, (char *)NULL);
        // If exec fails:
        fprintf(stderr, "as: %s\n", strerror(errno));
        _exit(1);
    }

    // 4. Parent: wait for assembler to finish.
    int status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        error("assembler failed");

    // 5. Clean up the temporary assembly file.
    unlink(asm_template);
}


