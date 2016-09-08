// RUN: %clang_cc1 -fsyntax-only -verify -std=c++14 %s
// RUN: %clang_cc1 -fsyntax-only -verify -freflection -std=c++14 %s -DREFLECTION_SWITCH
// expected-no-diagnostics

// Test only the compiler switch and feature define.

#ifdef REFLECTION_SWITCH
# if !defined(__cpp_reflection) || !(__cpp_reflection)
#  error Reflection support is required with the -freflection switch.
# endif
#else
# if defined(__cpp_reflection) && (__cpp_reflection)
#  error Reflection should not be enabled without the -freflection switch.
# endif
#endif
