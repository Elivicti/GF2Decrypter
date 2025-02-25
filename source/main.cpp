#include <CLI/CLI.hpp>

#include "Util.hpp"
#include "Decrypter.hpp"

// constexpr auto ENCRYPTION_KEY = "\x55\x6E\x69\x74\x79\x46\x53\x00\x00\x00\x00\x07\x35\x2E\x78\x2E";

using namespace std::string_literals;
using namespace std::string_view_literals;

struct DecrypterCli : public Decrypter
{
	using PathArray = std::vector<std::filesystem::path>;
	using PathSet   = std::set<std::filesystem::path>;

	DecrypterCli(CLI::App* app, const char* argv0)
		: input{}, output{ "output"sv }
		, jobs{ 2 }, suffix{ "bundle"s }, quiet{ false }
		, PROGRAM_DIR{ std::filesystem::path{ argv0 }.parent_path() }
		, SEARCH_PATHS{
			std::filesystem::path{ "."sv },
			PROGRAM_DIR,
			PROGRAM_DIR / ".."sv
		}
	{
		app->add_option("input"s, input)
			->description("input file or directory, if not specified, default search paths are used"s)
			->check(CLI::ExistingDirectory | CLI::ExistingFile)
			->take_all();
	
		app->add_option("-o,--output"s, output)
			->description("output directory"s)
			->check(CLI::ExistingDirectory | CLI::NonexistentPath)
			->default_val(output.string());
		app->add_option("-j,--jobs"s, jobs)
			->description("number of jobs"s)
			->default_val(2);
		app->add_option("-s,--suffix"s, suffix)
			->description("suffix of asset bundle files, only works if input is a directory"s)
			->default_val(suffix);
		app->add_flag("-q,--quiet"s, quiet)
			->description("supress console output"s);


		std::string footer{ "Default Search Paths:\n"s };
		for (auto& s : SEARCH_PATHS)
		{
			std::format_to(std::back_inserter(footer),
				"  {}\n", (s / FOLDER_NAME).generic_string());
		}
		app->footer(footer);

		app->callback([this]() { this->execute(); });
	}

	void decrpyt_file(const std::filesystem::path& file)
	{
		if (!std::filesystem::is_regular_file(file))
			return;

		auto output_file = output / file.filename();
		std::ifstream ifs{ file, std::ios::binary | std::ios::in };

		ByteArray data;
		ifs >> data;

		decrypt_bytes(data);
		std::ofstream ofs{ output_file, std::ios::binary | std::ios::out };
		ofs.write((char*)data.data(), data.size());

		print("{} -> {}\n", file.filename().string(), output_file.generic_string());
	}

	PathArray input;
	std::filesystem::path output;
	int jobs;
	std::string suffix;
	bool quiet;

	const std::filesystem::path PROGRAM_DIR;
	const std::filesystem::path SEARCH_PATHS[3];
	static constexpr auto FOLDER_NAME = "AssetBundles_Windows"sv;	

	std::filesystem::path get_default_path()
	{
		using namespace std::string_view_literals;
	
		for (auto& search : SEARCH_PATHS)
		{
			auto p = search / FOLDER_NAME;
			if (std::filesystem::is_directory(p))
				return std::filesystem::canonical(p);
		}
		throw std::runtime_error{ "No valid asset bundle folder found in default search path, please specify input" };
	};

	PathSet get_input_files(const PathArray& input)
	{
		PathSet ret;
		auto extension_match = std::format(".{}", suffix);

		if (input.empty())
		{
			auto iter = std::filesystem::directory_iterator{
				get_default_path(),
				std::filesystem::directory_options::skip_permission_denied
			};
			for (auto& f : iter)
			{
				auto path = f.path();
				if (path.extension().string() != extension_match)
					continue;
				ret.emplace(std::move(path));
			}
		}
		
		for (auto& path : input)
		{
			if (std::filesystem::is_regular_file(path))
			{
				ret.emplace(path);
				continue;
			}
			if (!std::filesystem::is_directory(path))
				continue;
			auto iter = std::filesystem::directory_iterator{
				path, std::filesystem::directory_options::skip_permission_denied
			};
			for (auto& f : iter)
			{
				auto path = f.path();
				if (path.extension().string() != extension_match)
					continue;
				ret.emplace(std::move(path));
			}
		}
		return ret;
	}

	template<typename ...Args>
	void print(std::format_string<Args...> fmt, Args&& ...args)
	{
		if (!quiet)
			util::print(std::cout, fmt, std::forward<Args>(args)...);
	}

	void execute();
};


void DecrypterCli::execute()
{
	if (!std::filesystem::exists(output))
		std::filesystem::create_directory(output);

	PathSet input_files = get_input_files(input);
	for (auto& file : input_files)
	{
		decrpyt_file(file);
	}
}



int main(int argc, char* argv[])
{
	CLI::App app{ "Decrypt asset bundle from Girl's Frontline 2: Exilum" };
	DecrypterCli decrypter{ &app, argv[0] };
	
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