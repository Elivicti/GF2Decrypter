#include <CLI/CLI.hpp>

#include "util.hpp"

int main(int argc, char* argv[])
{
	CLI::App app{ "decrypt asset bundle from Girl's Frontline 2: Exilum" };

	try
	{
		app.parse(argc, argv);
	}
	catch(const CLI::ParseError& e)
	{
		return app.exit(e);
	}
	catch(const std::runtime_error& e)
	{
		util::print(std::cerr, "{}\n", e.what());
		util::print(std::cerr, "Run with --help for more information.\n");
		return 1;
	}

	return 0;
}