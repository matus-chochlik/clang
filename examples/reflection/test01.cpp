#include <reflexpr>

int main(void)
{
	using namespace std;
	static_assert(meta::Object<reflexpr()>, "");
	static_assert(!meta::Reversible<reflexpr()>, "");
	static_assert(meta::GlobalScope<reflexpr()>, "");
	static_assert(meta::Namespace<reflexpr()>, "");
	static_assert(meta::Named<reflexpr()>, "");

	using mspc = reflexpr(static);
	static_assert(meta::Named<mspc>, "");
	return 0;
}
