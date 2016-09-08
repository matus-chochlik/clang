// RUN: %clang_cc1 -fsyntax-only -verify -freflection -std=c++14 %s
// expected-no-diagnostics

struct S {
    int i;
    static float f;
};
union U {
    int i;
    static float f;
};
class C {
public:
    int i;
    static float f;
};
enum E {
    En
};
enum class EC {
    En
};

namespace N {
    namespace NN {
        typedef int IntA;
        using IntB = int;
    }
}
using namespace N;

constexpr __metaobject_id __meta_builtins[] = {
    __reflexpr(void),
    __reflexpr(bool),
    __reflexpr(signed char),
    __reflexpr(unsigned char),
    __reflexpr(signed short),
    __reflexpr(signed short),
    __reflexpr(signed int),
    __reflexpr(signed long),
    __reflexpr(signed long long),
    __reflexpr(float),
    __reflexpr(double),
    __reflexpr(long double),
    __reflexpr(char),
    __reflexpr(wchar_t),
    __reflexpr(char16_t),
    __reflexpr(char32_t),
    __reflexpr(decltype(nullptr))
};

constexpr __metaobject_id __meta_udtypes[] = {
    __reflexpr(S),
    __reflexpr(U),
    __reflexpr(C),
    __reflexpr(E),
    __reflexpr(EC)
};

constexpr __metaobject_id __meta_namespaces[] = {
    __reflexpr(),
    __reflexpr(::),
    __reflexpr(N),
    __reflexpr(NN),
    __reflexpr(N::NN)
};

constexpr __metaobject_id __meta_members[] = {
    __reflexpr(S::i),
    __reflexpr(S::f),
    __reflexpr(C::i),
    __reflexpr(C::f),
    __reflexpr(U::i),
    __reflexpr(U::f),
    __reflexpr(::En),
    __reflexpr(EC::En),
    __reflexpr(N::NN::IntA),
    __reflexpr(N::NN::IntB)
};

constexpr __metaobject_id __meta_specifiers[] = {
    __reflexpr(struct),
    __reflexpr(class),
    __reflexpr(union),
    __reflexpr(enum),
    __reflexpr(static),
    __reflexpr(public),
    __reflexpr(protected),
    __reflexpr(private)
};

template <class T>
struct TypeTmp {
    static constexpr __metaobject_id __meta_arg = __reflexpr(T);
};
TypeTmp<int> t1;

template <int I>
struct NontypeTmp {
    static constexpr __metaobject_id __meta_arg = __reflexpr(I);
};
NontypeTmp<0> t2;

template <template <class> class T>
struct TmpTmp {
    static constexpr __metaobject_id __meta_arg = __reflexpr(T);
};
TmpTmp<TypeTmp> t3;
