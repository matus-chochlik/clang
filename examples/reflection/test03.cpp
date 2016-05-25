#include <reflexpr>
#include <iostream>

namespace foo {
namespace bar {
typedef int baz;

} // bar

struct qux {
	int a, b, c;
	static double d;
}; // qux

enum one {
	x, y, z
};

} // foo

int main(void)
{
	using namespace std::meta;
	std::cout << get_source_file_v<reflexpr(class)> << std::endl;
	std::cout << get_name_v<reflexpr(class)> << std::endl;
	std::cout << get_name_v<reflexpr(protected)> << std::endl;
	std::cout << get_display_name_v<reflexpr()> << std::endl;
	std::cout << is_anonymous_v<reflexpr()> << std::endl;
	std::cout << Namespace<reflexpr(std)> << std::endl;
	std::cout << Alias<reflexpr(std)> << std::endl;
	std::cout << get_name_v<reflexpr(std)> << std::endl;
	std::cout << get_display_name_v<get_scope_m<reflexpr(std)>> << std::endl;
	std::cout << get_name_v<get_scope_m<reflexpr(foo::bar)>> << std::endl;
	std::cout << get_name_v<reflexpr(foo::bar)> << std::endl;
	std::cout << get_name_v<reflexpr(foo::bar::baz)> << std::endl;
	std::cout << Namespace<get_scope_m<reflexpr(foo::bar)>> << std::endl;
	std::cout << GlobalScope<get_scope_m<get_scope_m<reflexpr(foo::bar)>>> << std::endl;
	std::cout << get_source_file_v<reflexpr(foo::bar)> << std::endl;
	std::cout << get_source_line_v<reflexpr(foo::bar)> << std::endl;
	std::cout << get_source_column_v<reflexpr(foo::bar)> << std::endl;
	std::cout << get_name_v<reflexpr(unsigned int)> << std::endl;
	std::cout << get_name_v<reflexpr(std::size_t)> << std::endl;
	std::cout << get_name_v<reflexpr(decltype(std::size_t(0)))> << std::endl;
	std::cout << get_display_name_v<reflexpr(std::string)> << std::endl;
	std::cout << get_name_v<get_elaborated_type_specifier_m<reflexpr(std::string)>> << std::endl;
	std::cout << ObjectSequence<get_data_members_m<reflexpr(std::string)>> << std::endl;
	std::cout << get_size_v<get_data_members_m<reflexpr(foo::qux)>> << std::endl;
	std::cout << get_name_v<get_element_m<get_data_members_m<reflexpr(foo::qux)>, 1>> << std::endl;
	std::cout << get_name_v<get_type_m<get_element_m<get_data_members_m<reflexpr(foo::qux)>, 1>>> << std::endl;
std::cout << get_name_v<get_access_specifier_m<get_element_m<get_data_members_m<reflexpr(foo::qux)>, 1>>> << std::endl;
	std::cout << get_name_v<get_scope_m<get_element_m<get_enumerators_m<reflexpr(foo::one)>, 1>>> << std::endl;
	std::cout << get_name_v<get_element_m<get_enumerators_m<reflexpr(foo::one)>, 1>> << std::endl;
	std::cout << get_name_v<get_type_m<get_element_m<get_enumerators_m<reflexpr(foo::one)>, 1>>> << std::endl;

	get_reflected_type_t<reflexpr(foo::one)> o;
	std::cout << sizeof(o) << std::endl;

	return 0;
}
