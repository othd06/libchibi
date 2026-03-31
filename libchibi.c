#define _GNU_SOURCE
#include "chibicc.h"

#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>


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

Type* get_return_type(Type* func_type) {
    if (func_type->kind != TY_FUNC) error("attempting to get return type of non-function type");
    return func_type->return_ty;
}

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

Type* create_function_type_full(Type* return_ty, Type** params, bool is_variadic) {
    Type* funtype = calloc(1, sizeof(Type));
    funtype->kind = TY_FUNC;
    funtype->return_ty = return_ty;
    funtype->params = *params;
    funtype->is_variadic = is_variadic;
    funtype->next = NULL;
    return funtype;
}
Type* create_function_type(Type* return_ty, Type** params) {return create_function_type_full(return_ty, params, false);}

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

Type* create_variable_length_array_type(Type* type, Node* len_node) {
    Type* vlatype = calloc(1, sizeof(Type));
    vlatype->kind = TY_VLA;
    vlatype->align = type->align;
    vlatype->base = type;
    vlatype->vla_len = len_node;
    {
        Obj* size = calloc(1, sizeof(Obj));
        size->next = NULL;
        size->name = NULL;
        size->ty = create_base_type(BTY_LONG);
        size->is_local = true;
        size->is_function = false;
        size->align = size->ty->align;
        vlatype->vla_size = size;
    }
    vlatype->next = NULL;
    return vlatype;
}

void append_unpacked_struct_type_member(Member** list, Type* type) {
    if (*list == NULL) {
        Member* member = calloc(1, sizeof(Member));
        member->next = NULL;
        member->ty = type;
        member->idx = 0;
        member->align = type->align;
        member->offset = 0;
        *list = member;
        return;
    }
    Member* current = *list;
    while (current->next != NULL) current = current->next;
    if (current->ty->kind == TY_ARRAY && current->ty->array_len == 0) error("cannot append to the layout of a flexible struct\n");
    Member* member = calloc(1, sizeof(Member));
    member->next = NULL;
    member->ty = type;
    member->idx = current->idx+1;
    member->align = type->align;
    member->offset = current->offset+current->ty->size;
    while (member->offset % type->align != 0) {member->offset += 1;}
    current->next = member;
}

void append_unpacked_struct_bitfield_member(Member** list, Type* type, int width) {
    if (!(type->kind == TY_CHAR || type->kind == TY_SHORT || type->kind == TY_INT)) error("can only make a bitfield using types: char, short, int");
    if (width > type->size*8) error("bitfield width greater than type size");
    if (*list == NULL) {
        if (width == 0) return;
        Member* member = calloc(1, sizeof(Member));
        member->next = NULL;
        member->ty = type;
        member->idx = 0;
        member->align = type->align;
        member->offset = 0;
        member->is_bitfield = true;
        member->bit_offset = 0;
        member->bit_width = width;
        *list = member;
        return;
    }
    Member* current = *list;
    while (current->next != NULL) current = current->next;
    if (current->ty->kind == TY_ARRAY && current->ty->array_len == 0) error("cannot append to the layout of a flexible struct\n");
    if (current->ty->kind == type->kind && current->is_bitfield && current->bit_offset+current->bit_width+width <= current->ty->size*8 && current->bit_width != 0) {
        Member* member = calloc(1, sizeof(Member));
        member->next = NULL;
        member->ty = type;
        member->idx = current->idx+1;
        member->align = type->align;
        member->offset = current->offset;
        member->is_bitfield = true;
        member->bit_offset = current->bit_offset+current->bit_width;
        member->bit_width = width;
        current->next = member;
        return;
    }
    if (width == 0) return;
    Member* member = calloc(1, sizeof(Member));
    member->next = NULL;
    member->ty = type;
    member->idx = current->idx+1;
    member->align = type->align;
    member->offset = current->offset+current->ty->size;
    while (member->offset % type->align != 0) {member->offset += 1;}
    member->is_bitfield = true;
    member->bit_offset = 0;
    member->bit_width = width;
    current->next = member;
}

void append_packed_struct_type_member(Member** list, Type* type) {
    if (*list == NULL) {
        Member* member = calloc(1, sizeof(Member));
        member->next = NULL;
        member->ty = type;
        member->idx = 0;
        member->align = type->align;
        member->offset = 0;
        *list = member;
        return;
    }
    Member* current = *list;
    while (current->next != NULL) current = current->next;
    if (current->ty->kind == TY_ARRAY && current->ty->array_len == 0) error("cannot append to the layout of a flexible struct\n");
    Member* member = calloc(1, sizeof(Member));
    member->next = NULL;
    member->ty = type;
    member->idx = current->idx+1;
    member->align = type->align;
    member->offset = current->offset+current->ty->size;
    current->next = member;
}

void append_packed_struct_bitfield_member(Member** list, Type* type, int width) {
    if (!(type->kind == TY_CHAR || type->kind == TY_SHORT || type->kind == TY_INT)) error("can only make a bitfield using types: char, short, int");
    if (width > type->size*8) error("bitfield width greater than type size");
    if (*list == NULL) {
        if (width == 0) return;
        Member* member = calloc(1, sizeof(Member));
        member->next = NULL;
        member->ty = type;
        member->idx = 0;
        member->align = type->align;
        member->offset = 0;
        member->is_bitfield = true;
        member->bit_offset = 0;
        member->bit_width = width;
        *list = member;
        return;
    }
    Member* current = *list;
    while (current->next != NULL) current = current->next;
    if (current->ty->kind == TY_ARRAY && current->ty->array_len == 0) error("cannot append to the layout of a flexible struct\n");
    if (current->ty->kind == type->kind && current->is_bitfield && current->bit_offset+current->bit_width+width <= current->ty->size*8 && current->bit_width != 0) {
        Member* member = calloc(1, sizeof(Member));
        member->next = NULL;
        member->ty = type;
        member->idx = current->idx+1;
        member->align = type->align;
        member->offset = current->offset;
        member->is_bitfield = true;
        member->bit_offset = current->bit_offset+current->bit_width;
        member->bit_width = width;
        current->next = member;
        return;
    }
    if (width == 0) return;
    Member* member = calloc(1, sizeof(Member));
    member->next = NULL;
    member->ty = type;
    member->idx = current->idx+1;
    member->align = type->align;
    member->offset = current->offset+current->ty->size;
    member->is_bitfield = true;
    member->bit_offset = 0;
    member->bit_width = width;
    current->next = member;
}

void append_union_member_type(Member** list, Type* type) {
    if (*list == NULL) {
        Member* member = calloc(1, sizeof(Member));
        member->next = NULL;
        member->ty = type;
        member->idx = 0;
        member->align = type->align;
        member->offset = 0;
        *list = member;
        return;
    }
    Member* current = *list;
    while (current->next != NULL) {current = current->next;}
    Member* member = calloc(1, sizeof(Member));
    member->next = NULL;
    member->ty = type;
    member->idx = current->idx+1;
    member->align = type->align;
    member->offset = 0;
    current->next = member;
}

Type* create_struct_type_full(Member** members, bool is_packed) {
    Type* structtype = calloc(1, sizeof(Type));
    structtype->kind = TY_STRUCT;
    {
        Member* current = *members;
        while (current->next != NULL) {current = current->next;}
        structtype->size = current->offset + current->ty->size;
    }
    {
        Member* current = *members;
        int align = 0;
        while (current->next != NULL) {
            if (current->align > align) align = current->align;
            current = current->next;
        }
        if (current->align > align) align = current->align;
        if (is_packed) align = 1;
        else while (structtype->size % align != 0) structtype->size += 1;
        structtype->align = align;
    }
    structtype->members = *members;
    structtype->is_packed = is_packed;
    {
        Member* current = *members;
        while (current->next != NULL) {current = current->next;}
        structtype->is_flexible = current->ty->kind == TY_ARRAY && current->ty->array_len == 0;
    }
    structtype->next = NULL;
    return structtype;
}
Type* create_struct_type(Member** members) {return create_struct_type_full(members, false);}

Type* create_union_type_full(Member** members, bool is_packed) {
    Type* uniontype = calloc(1, sizeof(Type));
    uniontype->kind = TY_UNION;
    {
        Member* current = *members;
        int size = 0;
        while (current->next != NULL) {
            if (current->ty->size > size) size = current->ty->size;
            current = current->next;
        }
        if (current->ty->size > size) size = current->ty->size;
        uniontype->size = size;
    }
    {
        Member* current = *members;
        int align = 0;
        while (current->next != NULL) {
            if (current->align > align) align = current->align;
            current = current->next;
        }
        if (current->align > align) align = current->align;
        if (is_packed) align = 1;
        uniontype->align = align;
    }
    uniontype->members = *members;
    uniontype->is_packed = is_packed;
    uniontype->next = NULL;
    return uniontype;
}
Type* create_union_type(Member** members) {return create_union_type_full(members, false);}

void append_type(Type** list, Type* type) {
    if (*list == NULL) {*list = type; return;}
    Type* current = *list;
    while (current->next != NULL) current = current->next;
    current->next = type;
}

void traverse_node(Node* node, Obj* function) {
    if (node == NULL) return;
    switch (node->kind) {
        case (ND_NULL_EXPR): {
            break;
        } case (ND_ADD): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_SUB): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_MUL): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_DIV): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_NEG): {
            traverse_node(node->lhs, function);
            break;
        } case (ND_MOD): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_BITAND): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_BITOR): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_BITXOR): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_SHL): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_SHR): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_EQ): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_NE): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_LT): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_LE): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_ASSIGN): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_COND): {
            traverse_node(node->cond, function);
            traverse_node(node->then, function);
            traverse_node(node->els, function);
            break;
        } case (ND_COMMA): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_MEMBER): {
            break;
        } case (ND_ADDR): {
            traverse_node(node->lhs, function);
            break;
        } case (ND_DEREF): {
            traverse_node(node->lhs, function);
            break;
        } case (ND_NOT): {
            traverse_node(node->lhs, function);
            break;
        } case (ND_BITNOT): {
            traverse_node(node->lhs, function);
            break;
        } case (ND_LOGAND): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_LOGOR): {
            traverse_node(node->lhs, function);
            traverse_node(node->rhs, function);
            break;
        } case (ND_RETURN): {
            traverse_node(node->lhs, function);
            break;
        } case (ND_IF): {
            traverse_node(node->cond, function);
            traverse_node(node->then, function);
            traverse_node(node->els, function);
            break;
        } case (ND_FOR): {
            traverse_node(node->init, function);
            traverse_node(node->cond, function);
            traverse_node(node->inc, function);
            traverse_node(node->body, function);
            break;
        } case (ND_DO): {
            traverse_node(node->body, function);
            traverse_node(node->cond, function);
            break;
        } case (ND_SWITCH): {
            traverse_node(node->case_next, function);
            traverse_node(node->default_case, function);
            traverse_node(node->body, function);
            break;
        } case (ND_CASE): {
            traverse_node(node->next, function);
            break;
        } case (ND_BLOCK): {
            Node* current = node->body;
            while (current != NULL) {
                traverse_node(current, function);
                current = current->next;
            }
            break;
        } case (ND_GOTO): {
            break;
        } case (ND_GOTO_EXPR): {
            break;
        } case (ND_LABEL): {
            break;
        } case (ND_LABEL_VAL): {
            break;
        } case (ND_FUNCALL): {
            break;
        } case (ND_EXPR_STMT): {
            traverse_node(node->lhs, function);
            break;
        } case (ND_STMT_EXPR): {
            traverse_node(node->body, function);
            break;
        } case (ND_VAR): {
            if (node->label != NULL) {
                Obj* local_var;
                {
                    Obj* current = function->locals;
                    if (current == NULL) error("attempting to extract nonexistent local variable");
                    while(strcmp(current->name, node->label) != 0) {
                        current = current->next;
                        if (current == NULL) error("attempting to extract nonexistent local variable");
                    }
                    local_var = current;
                }
                node->ty = local_var->ty;
                node->var = local_var;
                if (local_var->ty->kind == TY_VLA) node->kind = ND_VLA_PTR;
                node->label = NULL;
            }
            break;
        } case (ND_VLA_PTR): {
            if (node->label != NULL) {
                Obj* local_var;
                {
                    Obj* current = function->locals;
                    if (current == NULL) error("attempting to extract nonexistent local variable");
                    while(strcmp(current->name, node->label) != 0) {
                        current = current->next;
                        if (current == NULL) error("attempting to extract nonexistent local variable");
                    }
                    local_var = current;
                }
                node->ty = local_var->ty;
                node->var = local_var;
                node->label = NULL;
            }
            break;
        } case (ND_NUM): {
            break;
        } case (ND_CAST): {
            traverse_node(node->lhs, function);
            break;
        } case (ND_MEMZERO): {
            traverse_node(node->lhs, function);
            break;
        } case (ND_ASM): {
            break;
        } case (ND_CAS): {
            traverse_node(node->cas_addr, function);
            traverse_node(node->cas_old, function);
            traverse_node(node->cas_new, function);
            break;
        } case (ND_EXCH): {
            traverse_node(node->cas_addr, function);
            traverse_node(node->cas_new, function);
            break;
        }
    }
}

Obj* create_function_definition_full(char* name, Type* func_type, int argc, char* argv[], int localc, Type** local_types, char* local_names[], Node* body_node, bool is_variadic, bool is_local, bool is_static, bool is_inline, bool is_live) {
    Obj* definition = calloc(1, sizeof(Obj));
    definition->next = NULL;
    definition->name = name;
    definition->ty = func_type;
    definition->is_function = true;
    definition->is_definition = true;
    definition->is_static = is_static;
    definition->is_inline = is_inline;
    {
        if (is_variadic) {
            Obj* va_area = calloc(1, sizeof(Obj));

            va_area->next = NULL;
            va_area->name = "__va_area";
            va_area->ty = create_array_type(create_base_type(BTY_CHAR), 0);
            va_area->is_local = true;
            va_area->align = va_area->ty->align;

            definition->va_area = va_area;
        } else definition->va_area = NULL;
    }
    {
        Type* current = func_type->params;
        int index = 0;
        Obj* params = NULL;
        while (index < argc) {
            if (current == NULL) error("number of arguments in function signature less than number of argument names given");
            Obj* param = calloc(1, sizeof(Obj));
            param->name = argv[index];
            param->ty = current;
            param->is_local = true;
            param->align = current->align;
            param->next = NULL;
            Obj* currentObj = params;
            if (params == NULL) params = param;
            else {while (currentObj->next != NULL) currentObj = currentObj->next; currentObj->next = param;}

            index += 1;
            current = current->next;
        }
        if (current != NULL) error("number of arguments in function signature more than number of argument names given");
        definition->params = params;
    }
    {
        Obj* alloca_bottom = calloc(1, sizeof(Obj));

        alloca_bottom->next = NULL;
        alloca_bottom->name = "__alloca_bottom";
        alloca_bottom->ty = create_ptr_type(create_base_type(BTY_CHAR));
        alloca_bottom->is_local = true;
        alloca_bottom->align = alloca_bottom->ty->align;

        definition->alloca_bottom = alloca_bottom;
    }
    {

        {
            definition->locals = NULL;
            Type* current = NULL;
            if (localc > 0) current = *local_types;
            for (int i = 0; i < localc; i++) {
                if (current == NULL) error("localc greater than provided number of local types");
                if (local_names[i] == NULL) error("localc greater than the provided number of local names");

                Obj* new_local = calloc(1, sizeof(Obj));
                new_local->next = definition->locals;
                new_local->name = local_names[i];
                new_local->ty = current;
                new_local->is_local = true;
                new_local->align = current->align;

                definition->locals = new_local;
                current = current->next;
            }
            if (current != NULL) error("localc lower than provided number of local types");
        }
        
        {
            Obj* current = definition->locals;

            if (definition->locals == NULL) {
                definition->locals = definition->params;
                current = definition->params;
            }
            else {
                while (current->next != NULL) current = current->next;
                current->next = definition->params;
            }
            if (definition->locals == NULL) {
                definition->locals = definition->va_area;
                current = definition->va_area;
            }
            else {
                while (current->next != NULL) current = current->next;
                current->next = definition->va_area;
            }
            if (definition->locals == NULL) {
                definition->locals = definition->alloca_bottom;
                current = definition->alloca_bottom;
            }
            else {
                while (current->next != NULL) current = current->next;
                current->next = definition->alloca_bottom;
            }
        }
        
    }
    definition->body = body_node;
    definition->is_live = true;
    if (is_static && is_inline) {
        definition->is_live = is_live;
    }
    definition->is_local = is_local;
    traverse_node(definition->body, definition);
    return definition;
}
Obj* create_function_definition(char* name, Type* func_type, int argc, char* argv[], int localc, Type** local_types, char* local_names[], Node* body_node) {return create_function_definition_full(name, func_type, argc, argv, localc, local_types, local_names, body_node, false, false, false, false, true);}

Obj* create_function_declaration_full(char* name, Type* ret_type, bool is_static) {
    Obj* declaration = calloc(1, sizeof(Obj));
    declaration->next = NULL;
    declaration->name = name;
    declaration->ty = ret_type;
    declaration->is_function = true;
    declaration->is_definition = false;
    declaration->is_static = is_static;
    return declaration;
}
Obj* create_function_declaration(char* name, Type* ret_type) {return create_function_declaration_full(name, ret_type, false);}

typedef struct {
    char* init_data;
    Relocation* rel;
} GlobalInit;

typedef struct {
    char* name;
    long addend;
} PtrInitData;

typedef struct {
    Type* union_type;
    Type* data_type;
    void* data;
} UnionInitData;

GlobalInit create_base_type_initialiser(void* data, int size) {
    GlobalInit initialiser;
    initialiser.init_data = memcpy(calloc(1, size), data, size);
    initialiser.rel = NULL;
    return initialiser;
}

GlobalInit create_ptr_initialiser(PtrInitData data) {
    GlobalInit initialiser;
    initialiser.init_data = calloc(1, 8);
    initialiser.rel = calloc(1, sizeof(Relocation));
    initialiser.rel->next = NULL;
    initialiser.rel->offset = 0;
    initialiser.rel->label = calloc(1, 8);
    initialiser.rel->label = memcpy(calloc(1, strlen(data.name)+1), data.name, strlen(data.name)+1);
    initialiser.rel->addend = data.addend;
    return initialiser;
}

GlobalInit create_struct_initialiser(Type* type, void* data[]);
GlobalInit create_union_initialiser(UnionInitData data);

GlobalInit create_array_initialiser(Type* type, void* data) {
    //call with a pointer to an array of either:
        //the raw datas (for raw datatypes)
        //PtrInitDatas (for raw pointers)
        //data pointers (for nested arrays)
        //pointers to arrays of pointers (for structs)
        //arrays of UnionInitDatas (for structs)
    GlobalInit initialiser;
    initialiser.init_data = calloc(1, type->size);
    initialiser.rel = NULL;
    for (int i = 0; i < type->array_len; i++) {
        GlobalInit comp_init;
        if (type->base->kind == TY_UNION) {
            UnionInitData* arr = data;
            UnionInitData elem = arr[i];
            comp_init = create_union_initialiser(elem);
        } else if (type->base->kind == TY_STRUCT) {
            void*** arr = data;
            void** elem = arr[i];
            comp_init = create_struct_initialiser(type->base, elem);
        } else if (type->base->kind == TY_ARRAY) {
            void** arr = data;
            void* subarray = arr[i];
            comp_init = create_array_initialiser(type->base, subarray);
        } else if (type->base->kind == TY_PTR) {
            PtrInitData* arr = data;
            PtrInitData elem = arr[i];
            comp_init = create_ptr_initialiser(elem);
        } else {
            void** arr = data;
            void* elem = arr[i];
            comp_init = create_base_type_initialiser(elem, type->base->size);
        }
        
        memcpy(initialiser.init_data + i*type->base->size, comp_init.init_data, type->base->size);

        Relocation* current = comp_init.rel;
        if (current != NULL) {
            while (current->next != NULL) {
                current->offset += i*type->base->size;
                current = current->next;
            }
            current->offset += i*type->base->size;
            current->next = initialiser.rel;
            initialiser.rel = comp_init.rel;
        }
    }
    return initialiser;
}

GlobalInit create_union_initialiser(UnionInitData data) {
    GlobalInit initialiser;
    
    initialiser.init_data = calloc(1, data.union_type->size);
    
    GlobalInit comp_init;
    if (data.data_type->kind == TY_UNION) {
        comp_init = create_union_initialiser(*((UnionInitData*)data.data));
    } else if (data.data_type->kind == TY_STRUCT) {
        comp_init = create_struct_initialiser(data.data_type, (void**)data.data);
    } else if (data.data_type->kind == TY_ARRAY) {
        comp_init = create_array_initialiser(data.data_type, data.data);
    } else if (data.data_type->kind == TY_PTR) {
        comp_init = create_ptr_initialiser(*((PtrInitData*)data.data));
    } else {
        comp_init = create_base_type_initialiser(data.data, data.data_type->size);
    }
    memcpy(initialiser.init_data, comp_init.init_data, data.data_type->size);
    initialiser.rel = comp_init.rel;

    return initialiser;
}

GlobalInit create_struct_initialiser(Type* type, void* data[]) {
    //call with the type of the struct and an array of pointers to either:
        //The raw data (for raw datatypes)
        //A PtrInitData (for raw pointers)
        //An array of initialisation data (for arrays)
        //An array of pointers (for nested structs)
        //A UnionInitData (for unions)
        //An integer with its least significant bits representing the bitfield (for bitfields)
    GlobalInit initialiser;
    
    initialiser.init_data = calloc(1, type->size);
    initialiser.rel = NULL;

    int offset = 0;
    int index = 0;
    Member* current = type->members;

    while (current != NULL) {
        GlobalInit comp_init;
        int size = 0;
        int align_offset = 0;

        if (current->is_bitfield) {
            int bitfields = 0;
            while (current->next != NULL && current->next->offset == current->offset) {
                int** arr = *data;
                int elem = *arr[index];
                elem = elem & (0xFFFFFFFF >> (32-current->bit_width));
                elem = elem >> current->bit_offset;
                bitfields = bitfields | elem;

                index += 1;
                current = current->next;
            }
            int** arr = *data;
            int elem = *arr[index];
            elem = elem & (0xFFFFFFFF >> (32-current->bit_width));
            elem = elem >> current->bit_offset;
            bitfields = bitfields | elem;

            align_offset = current->offset-offset;
            if (current->ty->kind == TY_CHAR) {
                comp_init = create_base_type_initialiser(&bitfields, 1);
            } else if (current->ty->kind == TY_SHORT) {
                comp_init = create_base_type_initialiser(&bitfields, 2);
            } else if (current->ty->kind == TY_INT) {
                comp_init = create_base_type_initialiser(&bitfields, 4);
            }
            size = current->ty->size;
        }else if (current->ty->kind == TY_UNION) {
            align_offset = current->offset-offset;
            comp_init = create_union_initialiser(*((UnionInitData*)data[index]));
            size = current->ty->size;
        }else if (current->ty->kind == TY_STRUCT) {
            align_offset = current->offset-offset;
            comp_init = create_struct_initialiser(current->ty, (void**)data[index]);
            size = current->ty->size;
        } else if (current->ty->kind == TY_ARRAY) {
            align_offset = current->offset-offset;
            comp_init = create_array_initialiser(current->ty, data[index]);
            size = current->ty->size;
        } else if (current->ty->kind == TY_PTR) {
            align_offset = current->offset-offset;
            comp_init = create_ptr_initialiser(*(PtrInitData*)data[index]);
            size = 8;
        } else {
            align_offset = current->offset-offset;
            comp_init = create_base_type_initialiser(data[index], current->ty->size);
            size = current->ty->size;
        }
        offset += align_offset;
        memcpy(initialiser.init_data + offset, comp_init.init_data, size);

        Relocation* current_rel = comp_init.rel;
        if (current_rel != NULL) {
            while (current_rel->next != NULL) {
                current_rel->offset += offset;
                current_rel = current_rel->next;
            }
            current_rel->offset += offset;
            current_rel->next = initialiser.rel;
            initialiser.rel = comp_init.rel;
        }

        current = current -> next;
        index += 1;
        offset += size;
    }

    return initialiser;
}

Obj* create_global_variable_definition_full(char* name, Type* type, GlobalInit initialiser, bool is_static, bool is_tls) {
    Obj* definition = calloc(1, sizeof(Obj));
    definition->next = NULL;
    definition->name = name;
    definition->is_function = false;
    definition->ty = type;
    definition->is_static = is_static;
    definition->is_tls = is_tls;
    definition->is_tentative = false;
    definition->init_data = initialiser.init_data;
    definition->rel = initialiser.rel;
    definition->is_local = false;
    return definition;
}
Obj* create_global_variable_definition(char* name, Type* type, GlobalInit initialiser) {return create_global_variable_definition_full(name, type, initialiser, false, false);}

Obj* create_global_variable_declaration_full(char* name, Type* type, bool is_static, bool is_tls, bool is_extern) {
    Obj* declaration = calloc(1, sizeof(Obj));
    declaration->next = NULL;
    declaration->name = name;
    declaration->is_function = false;
    declaration->ty = type;
    declaration->is_static = is_static;
    declaration->is_tls = is_tls;
    declaration->init_data = NULL;
    declaration->is_tentative = !is_extern && !is_static;
    return declaration;
}
Obj* create_global_variable_declaration(char* name, Type* type) {return create_global_variable_declaration_full(name, type, false, false, false);}

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

void register_debug_files(DebugFile *files, int count) {debug_files = files; debug_file_count = count;}
void set_fpic(bool enabled) {opt_fpic = enabled;}
void set_fcommon(bool enabled) {opt_fcommon = enabled;}

static FILE *open_file(char *path) {
    if (!path || strcmp(path, "-") == 0)
        return stdout;

    FILE *out = fopen(path, "w");
    if (!out)
        error("cannot open output file: %s: %s", path, strerror(errno));
    return out;
}

void cProg(Obj** prog, char *output_file) {
    // Open a temporary output buffer.
    char *buf;
    size_t buflen;
    FILE *output_buf = open_memstream(&buf, &buflen);

    // Traverse the AST to emit assembly.
    codegen(*prog, output_buf);
    fclose(output_buf);

    // Write the asembly text to a file.
    FILE *out = open_file(output_file);
    fwrite(buf, buflen, 1, out);
    fclose(out);
    //{
    //    printf("running cat\n");
    //    pid_t pid = fork();
    //    if (pid < 0)
    //        error("fork failed: %s", strerror(errno));
    //    if (pid == 0) {
    //        //child: exec 'cat'
    //        execlp("cat", "cat", output_file, NULL);
    //        fprintf(stderr, "cat: %s\n", strerror(errno));
    //        _exit(1);
    //    }
    //    int status;
    //    waitpid(pid, &status, 0);
    //    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    //        error("cat failed");
    //}
}

void aProg(Obj** prog, char *output_file) {
    // 1. Create a temporary file for the assembly output.
    char asm_template[] = "/tmp/libchibi-XXXXXX.s";

    int fd = mkstemps(asm_template, 2);
    if (fd < 0)
        error("mkstemps failed: %s", strerror(errno));

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



Node* create_null_expression_node(int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NULL_EXPR;
    node->next = NULL;
    node->ty = create_base_type(BTY_VOID);
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_add_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_ADD;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_sub_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_SUB;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_mul_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_MUL;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_div_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_DIV;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_neg_node(Type* type, Node* value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NEG;
    node->next = NULL;
    node->ty = type;
    node->lhs = value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_mod_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_MOD;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_bit_and_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_BITAND;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_bit_or_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_BITOR;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_bit_xor_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_BITXOR;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_shl_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_SHL;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_shr_node(Type* type, Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_SHR;
    node->next = NULL;
    node->ty = type;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_eq_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_EQ;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_ne_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NE;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_lt_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_LT;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_le_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_LE;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_gt_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_LT;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->lhs = rhs;
    node->rhs = lhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_ge_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_LE;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->lhs = rhs;
    node->rhs = lhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_ass_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_ASSIGN;
    node->next = NULL;
    node->ty = lhs->ty;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_cond_node(Type* type, Node* cond_node, Node* then_node, Node* else_node, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_COND;
    node->next = NULL;
    node->ty = type;
    node->cond = cond_node;
    node->then = then_node;
    node->els = else_node;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_comma_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_COMMA;
    node->next = NULL;
    node->ty = rhs->ty;
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_member_node(Node* struct_node, Type* struct_union_type, int member_idx, int file_num, int line_num) {
    Member* member;
    {
        member = struct_union_type->members;
        for (int i = 0; i < member_idx; i++) member = member->next;
    }
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_MEMBER;
    node->next = NULL;
    node->ty = member->ty;
    node->lhs = struct_node;
    node->member = member;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_addr_node(Node* value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_ADDR;
    node->next = NULL;
    node->ty = create_ptr_type(value->ty);
    node->lhs = value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_deref_node(Node* value, int file_num, int line_num) {
    if (value->ty->kind != TY_PTR) error("attempting to dereference non-pointer type");
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_DEREF;
    node->next = NULL;
    node->ty = value->ty->base;
    node->lhs = value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_not_node(Node* value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NOT;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->lhs = value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_bit_not_node(Type* type, Node* value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_BITNOT;
    node->next = NULL;
    node->ty = type;
    node->lhs = value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_logic_and_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_LOGAND;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_logic_or_node(Node* lhs, Node* rhs, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_LOGOR;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->lhs = lhs;
    node->rhs = rhs;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_return_node(Node* value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->next = NULL;
    node->ty = value->ty;
    node->lhs = value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_if_node(Node* cond_node, Node* then_node, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_IF;
    node->next = NULL;
    node->cond = cond_node;
    node->then = then_node;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_if_else_node(Node* cond_node, Node* then_node, Node* else_node, int file_num, int line_num) {
    Node* node = create_if_node(cond_node, then_node, file_num, line_num);
    node->els = else_node;
    return node;
}
int brk_count = 0;
int cont_count = 0;
Node* create_while_node(Node* cond_node, Node* body_node, int file_num, int line_num, char** brk_label, char** cont_label) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_FOR;
    node->next = NULL;
    node->cond = cond_node;
    node->body = body_node;
    node->init = create_null_expression_node(file_num, line_num);
    node->inc = create_null_expression_node(file_num, line_num);
    {
        int len = snprintf(NULL, 0, ".L.break%d", brk_count);
        *brk_label = calloc(1, len+1);
        snprintf(*brk_label, len+1, ".L.break%d", brk_count);
        brk_count += 1;
        node->brk_label = *brk_label;
    }
    {
        int len = snprintf(NULL, 0, ".L.cont%d", cont_count);
        *cont_label = calloc(1, len+1);
        snprintf(*cont_label, len+1, ".L.cont%d", cont_count);
        cont_count += 1;
        node->cont_label = *cont_label;
    }
    return node;
}
Node* create_for_node(Node* init_node, Node* cond_node, Node* inc_node, Node* body_node, int file_num, int line_num, char** brk_label, char** cont_label) {
    Node* node = create_while_node(cond_node, body_node, file_num, line_num, brk_label, cont_label);
    node->init = init_node;
    node->inc = inc_node;
    return node;
}
Node* create_do_node(Node* body_node, Node* cond_node, int file_num, int line_num, char** brk_label, char** cont_label) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_DO;
    node->next = NULL;
    node->cond = cond_node;
    node->body = body_node;
    {
        int len = snprintf(NULL, 0, ".L.break%d", brk_count);
        *brk_label = calloc(1, len+1);
        snprintf(*brk_label, len+1, ".L.break%d", brk_count);
        brk_count += 1;
        node->brk_label = *brk_label;
    }
    {
        int len = snprintf(NULL, 0, ".L.cont%d", cont_count);
        *cont_label = calloc(1, len+1);
        snprintf(*cont_label, len+1, ".L.cont%d", cont_count);
        cont_count += 1;
        node->cont_label = *cont_label;
    }
    return node;
}
Node* create_block_node(Node** body_node_list, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    node->next = NULL;
    node->body = *body_node_list;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
int case_count = 0;
Node* create_case_node(int begin, int end, Node** body_node_list, int file_num, int line_num, char** label) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_CASE;
    node->next = NULL;
    node->begin = begin;
    node->end = end;
    node->body = *body_node_list;
    {
        int len = snprintf(NULL, 0, ".L.case%d", case_count);
        *label = calloc(1, len+1);
        snprintf(*label, len+1, ".L.case%d", case_count);
        case_count += 1;
        node->label = *label;
    }
    return node;
}
Node* create_default_case_node(Node** body_node_list, int file_num, int line_num, char** label) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_DEFAULT_CASE;
    node->next = NULL;
    node->body = *body_node_list;
    node->begin = 0;
    node->end = 0;
    {
        int len = snprintf(NULL, 0, ".L.case%d", case_count);
        *label = calloc(1, len+1);
        snprintf(*label, len+1, ".L.case%d", case_count);
        case_count += 1;
        node->label = *label;
    }
    return node;
}
Node* create_switch_node(Node** cases_node_list, Node* cond_node, int file_num, int line_num, char** brk_label) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_SWITCH;
    node->next = NULL;
    node->cond = cond_node;
    Node** body_list = NULL;
    {
        Node* current_case_node = *cases_node_list;
        Node* current_body_node = current_case_node->body;
        while (current_case_node != NULL && current_case_node->next != NULL && current_body_node == NULL) {
            current_case_node = current_case_node->next;
            current_body_node = current_case_node->body;
        }

        body_list = calloc(1, sizeof(Node*));
        *body_list = current_body_node;

        while (current_case_node != NULL && current_case_node->next != NULL) {
            while (current_body_node->next != NULL) current_body_node = current_body_node->next;
            current_body_node->next = current_case_node->next->body;
            current_case_node = current_case_node->next;
        }
    }
    node->body = create_block_node(body_list, file_num, line_num);
    node->case_next = *cases_node_list;
    {
        Node* current = node->case_next;
        if (current->kind == ND_DEFAULT_CASE) {
            node->default_case = current;
            current->kind = ND_CASE;
            node->case_next = current->next;
            current = current->next;
        }
        while (current != NULL && current->next != NULL) {
            if (current->next->kind == ND_DEFAULT_CASE) {
                node->default_case = current->next;
                current->next->kind = ND_CASE;
                current->next = current->next->next;
                node->default_case->next = NULL;
                if (current->next == NULL) break;
            }
            current->case_next = current->next;
            current->next = NULL;
            current = current->case_next;
        }
    }
    {
        int len = snprintf(NULL, 0, ".L.break%d", brk_count);
        *brk_label = calloc(1, len+1);
        snprintf(*brk_label, len+1, ".L.break%d", brk_count);
        brk_count += 1;
        node->brk_label = *brk_label;
    }
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
int label_count = 0;
char** create_unique_label() {
    char** label = calloc(1, sizeof(char*));
    int len = snprintf(NULL, 0, ".L.label%d", label_count);
    *label = calloc(1, len+1);
    snprintf(*label, len+1, ".L.label%d", label_count);
    label_count += 1;
    return label;
}
Node* create_goto_node(char** unique_label, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_GOTO;
    node->next = NULL;
    node->unique_label = *unique_label;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_label_node(char** unique_label, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_LABEL;
    node->next = NULL;
    node->unique_label = *unique_label;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_label_val_node(char** unique_label, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_LABEL_VAL;
    node->next = NULL;
    node->unique_label = *unique_label;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_goto_expr_node(Node* expr_node, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_GOTO_EXPR;
    node->next = NULL;
    node->lhs = expr_node;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_function_node(Obj* function, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_VAR;
    node->ty = function->ty;
    node->next = NULL;
    node->var = function;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_local_var_node(char* name, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_VAR;
    node->label = memcpy(calloc(1, strlen(name)+1), name, strlen(name)+1); //used to link the node with the actual local variable later
    node->next = NULL;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_global_var_node(Obj* global_var, char* name, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_VAR;
    node->ty = global_var->ty;
    node->next = NULL;
    node->var = global_var;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_funcall_node(Node* function_node, Type* function_type, Node** arg_node_list, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_FUNCALL;
    node->next = NULL;
    node->lhs = function_node;
    node->args = *arg_node_list;
    node->func_ty = function_type;
    node->ty = function_type->return_ty;
    if (node->ty->size > 16) {
        node->pass_by_stack = true;
        node->ret_buffer = calloc(1, sizeof(Obj));
        {
            node->ret_buffer->name = NULL;
            node->ret_buffer->next = NULL;
            node->ret_buffer->ty = node->ty;
            node->ret_buffer->is_local = true;
            node->ret_buffer->align = node->ty->align;
        }
    } else {
        node->pass_by_stack = false;
        node->ret_buffer = NULL;
    }
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_expression_stmt_node(Node* expression_node, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_EXPR_STMT;
    node->next = NULL;
    node->ty = create_base_type(BTY_VOID);
    node->lhs = expression_node;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Type* get_block_type(Node* block_node) {
    if (block_node->kind != ND_BLOCK) error("attempting to create a block expression with non-block");
    Node* current = block_node->body;
    if (current == NULL) error("attempting to create block expression with empty body");
    while(current->next != NULL) current = current->next;
    if (current->kind == ND_BLOCK) return get_block_type(current);
    else if (current->kind != ND_EXPR_STMT) error("attempting to create block expression that does not end with an expression statement");
    else return current->lhs->ty;
}
Node* create_block_expression_node(Node** body_node_list, int file_num, int line_num) {
    Node* block_node = create_block_node(body_node_list, file_num, line_num);
    Type* type = get_block_type(block_node);
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_STMT_EXPR;
    node->next = NULL;
    node->ty = type;
    node->body = block_node;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_char_literal_node(char value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->next = NULL;
    node->ty = create_base_type(BTY_CHAR);
    node->val = (uint64_t)value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_short_literal_node(short value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->next = NULL;
    node->ty = create_base_type(BTY_SHORT);
    node->val = (uint64_t)value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_int_literal_node(int value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->next = NULL;
    node->ty = create_base_type(BTY_INT);
    node->val = (uint64_t)value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_long_literal_node(long value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->next = NULL;
    node->ty = create_base_type(BTY_LONG);
    node->val = (uint64_t)value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_float_literal_node(float value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->next = NULL;
    node->ty = create_base_type(BTY_FLOAT);
    node->fval = (long double)value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_double_literal_node(double value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->next = NULL;
    node->ty = create_base_type(BTY_DOUBLE);
    node->fval = (long double)value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_long_double_literal_node(long double value, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->next = NULL;
    node->ty = create_base_type(BTY_LDOUBLE);
    node->fval = (long double)value;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_c_cast_node(Type* target_type, Node* expression, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_CAST;
    node->next = NULL;
    node->ty = target_type;
    node->lhs = expression;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_memzero_node(Node* address_expression, long byte_width, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_MEMZERO;
    node->next = NULL;
    node->ty = create_base_type(BTY_VOID);
    node->lhs = address_expression;
    node->val = byte_width;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_asm_node(char* asm_str, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_ASM;
    node->next = NULL;
    node->asm_str = asm_str;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_atomic_cas_node(Node* ptr, Node* expected, Node* desired, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_CAS;
    node->next = NULL;
    node->ty = create_base_type(BTY_BOOL);
    node->cas_addr = ptr;
    node->cas_old = expected;
    node->cas_new = desired;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}
Node* create_atomic_exchange_node(Node* ptr, Node* new, int file_num, int line_num) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_EXCH;
    node->next = NULL;
    node->ty = new->ty;
    node->cas_addr = ptr;
    node->cas_new = new;
    node->file_num = file_num;
    node->line_num = line_num;
    return node;
}



void append_node(Node** list, Node* node) {
    if (*list == NULL) {*list = node; return;}
    Node* current = *list;
    while (current->next != NULL) current = current->next;
    current->next = node;
}


char get_char_literal_from_node(Node* node) {
    if (node->kind != ND_NUM) error("attempting to get char literal from non-literal node");
    if (node->ty->kind != TY_CHAR) error("attempting to get char literal from non-char node");
    return (char)node->val;
}
short get_short_literal_from_node(Node* node) {
    if (node->kind != ND_NUM) error("attempting to get short literal from non-literal node");
    if (node->ty->kind != TY_SHORT) error("attempting to get short literal from non-short node");
    return (short)node->val;
}
int get_int_literal_from_node(Node* node) {
    if (node->kind != ND_NUM) error("attempting to get int literal from non-literal node");
    if (node->ty->kind != TY_INT) error("attempting to get int literal from non-int node");
    return (int)node->val;
}
long get_long_literal_from_node(Node* node) {
    if (node->kind != ND_NUM) error("attempting to get long literal from non-literal node");
    if (node->ty->kind != TY_LONG) error("attempting to get long literal from non-long node");
    return (long)node->val;
}
float get_float_literal_from_node(Node* node) {
    if (node->kind != ND_NUM) error("attempting to get float literal from non-literal node");
    if (node->ty->kind != TY_FLOAT) error("attempting to get float literal from non-float node");
    return (float)node->fval;
}
double get_double_literal_from_node(Node* node) {
    if (node->kind != ND_NUM) error("attempting to get double literal from non-literal node");
    if (node->ty->kind != TY_DOUBLE) error("attempting to get double literal from non-double node");
    return (double)node->fval;
}
long double get_long_double_literal_from_node(Node* node) {
    if (node->kind != ND_NUM) error("attempting to get long double literal from non-literal node");
    if (node->ty->kind != TY_LDOUBLE) error("attempting to get long double literal from non-long-double node");
    return (long double)node->fval;
}
