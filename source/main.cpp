#include <CLI/CLI.hpp>
#include <BS_thread_pool.hpp>

#include "util.hpp"

#include "Commands.hpp"

int main(int argc, char* argv[])
{
	CLI::App app{ "Decrypt data from Girl's Frontline 2: Exilum" };

	constexpr std::string_view marking{
		"B" // stands for "Bundle"
#ifdef ENABLE_TABLE_DECRYPTION
		"T" // stands for "Table"
#endif
	};

	app.set_version_flag(
		"-v,--version",
		std::format("{}[{}] {} (commit: {})"
			, EXE_NAME, marking
			, PROJECT_VERSION
			, GIT_HASH),
		"Print version and exit"
	);
	BundleDecrypter bundle{
		app.add_subcommand("bundle", "Decrypt asset bundles"),
		argv[0]
	};
	TableDecrypter table{
#ifdef ENABLE_TABLE_DECRYPTION
		app.add_subcommand("table", "Decrypt string tables"),
#else
		app.add_subcommand("table", "Decrypt string tables (*not enabled)"),
#endif
		argv[0]

	};

	app.require_subcommand(1);
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
