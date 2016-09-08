#include <reflexpr>

int main(void)
{
	using X = reflexpr(int);
	__unrefltype(std::meta::__unwrap_id_v<X>) i = 0;
	return i;
}
