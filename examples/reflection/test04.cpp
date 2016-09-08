#include <reflexpr>
#include <iostream>

struct A { };
struct B { };
struct C { };
struct D : A, B, C { };

int main(void)
{
	using namespace std;
	using metaD = reflexpr(D);
	using metaBC = meta::get_base_classes_m<metaD>;
	std::cout << meta::get_size_v<metaBC> << std::endl;
	std::cout << meta::get_base_name_v<meta::get_base_class_m<meta::get_element_m<metaBC, 0>>> << std::endl;
	std::cout << meta::get_base_name_v<meta::get_base_class_m<meta::get_element_m<metaBC, 1>>> << std::endl;
	std::cout << meta::get_base_name_v<meta::get_base_class_m<meta::get_element_m<metaBC, 2>>> << std::endl;

	return 0;
}
