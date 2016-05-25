// RUN: %clang_cc1 -fsyntax-only -verify -freflection -std=c++14 %s
// expected-no-diagnostics

// Test only for the presence of built-in operations and they pseudo-signature.

constexpr __metaobject_id __meta_int = __reflexpr(int);
constexpr bool __meta_traits[] = {
    __metaobject_is_meta_obj_sequence(__meta_int),
    __metaobject_is_meta_inheritance(__meta_int),
    __metaobject_is_meta_reversible(__meta_int),
    __metaobject_is_meta_named(__meta_int),
    __metaobject_is_meta_typed(__meta_int),
    __metaobject_is_meta_scope(__meta_int),
    __metaobject_is_meta_scope_member(__meta_int),
    __metaobject_is_meta_enum_member(__meta_int),
    __metaobject_is_meta_record_member(__meta_int),
    __metaobject_is_meta_alias(__meta_int),
    __metaobject_is_meta_constant(__meta_int),
    __metaobject_is_meta_variable(__meta_int),
    __metaobject_is_meta_namespace(__meta_int),
    __metaobject_is_meta_global_scope(__meta_int),
    __metaobject_is_meta_type(__meta_int),
    __metaobject_is_meta_tag_type(__meta_int),
    __metaobject_is_meta_enum(__meta_int),
    __metaobject_is_meta_specifier(__meta_int),
    __metaobject_is_anonymous(__meta_int)
};
struct S { int j; };
constexpr __metaobject_id __meta_S = __reflexpr(S);

constexpr auto _meta_S_file = __metaobject_get_source_file(__meta_S);
constexpr int _meta_S_line = __metaobject_get_source_line(__meta_S);
constexpr int _meta_S_column = __metaobject_get_source_column(__meta_S);
constexpr auto __meta_S_name = __metaobject_get_name(__meta_S);
constexpr auto __meta_S_display_name = __metaobject_get_display_name(__meta_S);
constexpr __metaobject_id __meta_S_scope = __metaobject_get_scope(__meta_S);

constexpr __metaobject_id __meta_S_tag = __metaobject_get_tag_specifier(__meta_S);

constexpr bool __meta_tag_traits[] = {
    __metaobject_is_enum(__meta_S),
    __metaobject_is_class(__meta_S),
    __metaobject_is_struct(__meta_S),
    __metaobject_is_union(__meta_S)
};

constexpr __metaobject_id __meta_S_types = __metaobject_get_member_types(__meta_S);
constexpr __metaobject_id __meta_S_vars = __metaobject_get_member_variables(__meta_S);
constexpr __metaobject_id __meta_S_consts = __metaobject_get_member_constants(__meta_S);

constexpr int s = __metaobject_get_size(__meta_S_vars);
constexpr __metaobject_id __meta_S_element = __metaobject_get_element(__meta_S_vars, 0);

constexpr __metaobject_id __meta_S_vars_private_exposed = __metaobject_expose_private(__meta_S_types);
constexpr __metaobject_id __meta_S_vars_protected_exposed = __metaobject_expose_protected(__meta_S_types);

constexpr __metaobject_id __meta_S_element_access = __metaobject_get_access_specifier(__meta_S_element);
constexpr bool __meta_access_traits[] = { 
    __metaobject_is_public(__meta_S_element),
    __metaobject_is_protected(__meta_S_element),
    __metaobject_is_private(__meta_S_element)
};

constexpr __metaobject_id __meta_S_element_meta_type = __metaobject_get_type(__meta_S_element);
typedef __unrefltype(__meta_S_element_meta_type) __meta_S_element_type;
