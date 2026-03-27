#pragma once
#include <stdbool.h>


#define empty_list {.raw = calloc(1, sizeof(void*))}

typedef struct{void* raw;} UniqueLabel;
typedef struct {void* raw;} Node;
typedef struct {void* raw;} NodeList;
typedef struct {void* raw;} MemberList;
typedef struct {void* raw;} Type;
typedef struct {void* raw;} TypeList;
typedef struct {void* raw;} Object;
typedef struct {void* raw;} ObjectList;
typedef struct {void* raw;} Relocation;



void append_object(ObjectList list, Object object);
void resection_objects(ObjectList list);

typedef struct {
    const char *file_name;
    int file_num;
} DebugFile;
void register_debug_files(DebugFile* files, int count);
void set_fpic(bool enabled);
void set_fcommon(bool enabled);
void cProg(ObjectList prog, char* output_file);
void aProg(ObjectList prog, char* output_file);


UniqueLabel create_unique_label();

Node create_null_expression_node(int file_num, int line_num);
Node create_add_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_sub_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_mul_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_div_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_neg_node(Type type, Node value, int file_num, int line_num);
Node create_mod_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_bit_and_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_bit_or_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_bit_xor_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_shl_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_shr_node(Type type, Node lhs, Node rhs, int file_num, int line_num);
Node create_eq_node(Node lhs, Node rhs, int file_num, int line_num);
Node create_lt_node(Node lhs, Node rhs, int file_num, int line_num);
Node create_le_node(Node lhs, Node rhs, int file_num, int line_num);
Node create_gt_node(Node lhs, Node rhs, int file_num, int line_num);
Node create_ge_node(Node lhs, Node rhs, int file_num, int line_num);
Node create_ass_node(Node lhs, Node rhs, int file_num, int line_num);
Node create_cond_node(Type type, Node cond_node, Node then_node, Node else_node, int file_num, int line_num);
Node create_comma_node(Node lhs, Node rhs, int file_num, int line_num);
Node create_member_node(Node struct_node, Type struct_union_type, int member_idx, int file_num, int line_num);
Node create_addr_node(Node value, int file_num, int line_num);
Node create_deref_node(Node value, int file_num, int line_num);
Node create_not_node(Node value, int file_num, int line_num);
Node create_bit_not_node(Node value, int file_num, int line_num);
Node create_logic_and_node(Node lhs, Node rhs, int file_num, int line_num);
Node create_logic_or_node(Node lhs, Node rhs, int file_num, int line_num);
Node create_return_node(Node value, int file_num, int line_num);
Node create_if_node(Node cond_node, Node then_node, int file_num, int line_num);
Node create_if_else_node(Node cond_node, Node then_node, Node else_node, int file_num, int line_num);
Node create_while_node(Node cond_node, Node body_node, int file_num, int line_num, UniqueLabel brk_label, UniqueLabel cont_label);
Node create_for_node(Node init_node, Node cond_node, Node inc_node, Node body_node, int file_num, int line_num, UniqueLabel brk_label, UniqueLabel cont_label);
Node create_do_node(Node body_node, Node cond_node, int file_num, int line_num, UniqueLabel brk_label, UniqueLabel cont_label);
Node create_block_node(NodeList body_node_list, int file_num, int line_num);
Node create_case_node(int begin, int end, NodeList body_node_list, int file_num, int line_num, UniqueLabel label);
Node create_default_case_node(NodeList body_node_list, int file_num, int line_num, UniqueLabel label);
Node create_switch_node(NodeList cases_node_list, Node cond_node, int file_num, int line_num, UniqueLabel brk_label);
Node create_goto_node(UniqueLabel unique_label, int file_num, int line_num);
Node create_label_node(UniqueLabel unique_label, int file_num, int line_num);
Node create_label_val_node(UniqueLabel unique_label, int file_num, int line_num);
Node create_goto_expr_node(Node expr_node, int file_num, int line_num);
Node create_function_node(Object function, int file_num, int line_num);
Node create_local_var_node(char* name, int file_num, int line_num);
Node create_global_var_node(Object global_var, char* name, int file_num, int line_num);
Node create_funcall_node(Node function_node, Type function_type, NodeList arg_node_list, int file_num, int line_num);
Node create_expression_stmt_node(Node expression_node, int file_num, int line_num);
Node create_block_expression_node(NodeList body_node_list, int file_num, int line_num);
Node create_char_literal_node(char value, int file_num, int line_num);
Node create_short_literal_node(short value, int file_num, int line_num);
Node create_int_literal_node(int value, int file_num, int line_num);
Node create_long_literal_node(long value, int file_num, int line_num);
Node create_float_literal_node(float value, int file_num, int line_num);
Node create_double_literal_node(double value, int file_num, int line_num);
Node create_long_double_literal_node(long double value, int file_num, int line_num);
Node create_c_cast_node(Type target_type, Node expression, int file_num, int line_num);
Node create_memzero_node(Node address_expression, long byte_width, int file_num, int line_num);
Node create_asm_node(char* asm_str, int file_num, int line_num);
Node create_atomic_cas_node(Node ptr, Node expected, Node desired, int file_num, int line_num);
Node create_atomic_exchange_node(Node ptr, Node new, int file_num, int line_num);

void append_node(NodeList list, Node node);

void append_union_member_type(MemberList list, Type type);
void append_packed_struct_bitfield_member(MemberList list, Type type, int width);
void append_packed_struct_type_member(MemberList list, Type type);
void append_unpacked_struct_bitfield_member(MemberList list, Type type, int width);
void append_unpacked_struct_type_member(MemberList list, Type type);

typedef enum {
    BTY_VOID, BTY_BOOL,
    BTY_CHAR, BTY_SHORT,
    BTY_INT, BTY_LONG,
    BTY_FLOAT, BTY_DOUBLE,
    BTY_LDOUBLE } BaseType;
Type create_union_type(MemberList members);
Type create_union_type_full(MemberList members, bool is_packed);
Type create_struct_type(MemberList members);
Type create_struct_type_full(MemberList members, bool is_packed);
Type create_variable_length_array_type(Type type, Node len_node);
Type create_array_type(Type type, int len);
Type create_function_type(Type return_ty, TypeList params);
Type create_function_type_full(Type return_ty, TypeList params, bool is_variadic);
Type create_ptr_type(Type type);
Type create_enum_type();
Type create_base_type(BaseType type);

void append_type(TypeList list, Type type);

typedef struct {
    char* init_data;
    Relocation rel;
} GlobalInit;
typedef struct {
    char* name;
    long addend;
} PtrInitData;
typedef struct {
    Type union_type;
    Type data_type;
    void* data;
    /*
    data is a pointer to:
        The raw data (for raw datatype members)
        A PtrInitData (for raw pointer members)
        An array of initialisation data (for array members)
        An array of pointers (for struct members)
        A UnionInitData (for nested union members)
        An integer with its least significant bits representing the bitfield (for bitfield members)
    */
} UnionInitData;
GlobalInit create_base_type_initialiser(void* data, int size);
/*
data is a pointer to a value of the type that this will be used to initialise
*/
GlobalInit create_ptr_initialiser(PtrInitData data);
GlobalInit create_struct_initialiser(Type type, void* data[]);
/*
call with the type of the struct and an array of pointers to either:
    The raw data (for raw datatype members)
    A PtrInitData (for raw pointer members)
    An array of initialisation data (for array members)
    An array of pointers (for nested struct members)
    A UnionInitData (for union members)
    An integer with its least significant bits representing the bitfield (for bitfield members)
*/
GlobalInit create_union_initialiser(UnionInitData data);
GlobalInit create_array_initialiser(Type type, void* data);
/*
call with a pointer to an array of either:
    the raw datas (for raw datatypes)
    PtrInitDatas (for raw pointers)
    data pointers (for nested arrays)
    pointers to arrays of pointers (for structs)
    arrays of UnionInitDatas (for structs)
*/

Object create_global_variable_declaration(char* name, Type type);
Object create_global_variable_declaration_full(char* name, Type type, bool is_static, bool is_tls, bool is_extern);
Object create_global_variable_definition(char* name, Type type, GlobalInit initialiser);
Object create_global_variable_definition_full(char* name, Type type, GlobalInit initialiser, bool is_static, bool is_tls);
Object create_function_declaration(char* name, Type ret_type);
Object create_function_declaration_full(char* name, Type ret_type, bool is_static);
Object create_function_definition(char* name, Type func_type, int argc, char* argv[], int localc, TypeList local_types, char* local_names[], Node body_node);
Object create_function_definition_full(char* name, Type func_type, int argc, char* argv[], int localc, TypeList local_types, char* local_names[], Node body_node, bool is_variadic, bool is_local, bool is_static, bool is_inline, bool is_live);

