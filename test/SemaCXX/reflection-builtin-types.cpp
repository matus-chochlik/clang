// RUN: %clang_cc1 -fsyntax-only -verify -freflection -std=c++14 %s
// expected-no-diagnostics

// Test only for the presence of the built-in types.

static_assert(__has_builtin(__unpack_metaobject_seq), "");
__metaobject_id moid;
