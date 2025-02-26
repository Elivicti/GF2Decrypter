#include <CLI/CLI.hpp>
#include <BS_thread_pool.hpp>

#include "Util.hpp"
#include "Decrypter.hpp"

#include <unordered_set>

// constexpr auto ENCRYPTION_KEY = "\x55\x6E\x69\x74\x79\x46\x53\x00\x00\x00\x00\x07\x35\x2E\x78\x2E";

using namespace std::string_literals;
using namespace std::string_view_literals;

struct DecrypterCli : public Decrypter
{
	struct BundleFile
	{
		std::filesystem::path path;
		int idx; // index in input std::vector, refer to the file's directory
		         // -1 means no parent directory
	};

	using PathArray = std::vector<std::filesystem::path>;
	using PathSet   = std::set<std::filesystem::path>;
	using BundleArray = std::vector<BundleFile>;
	using BundleSet   = std::set<BundleFile>;

	DecrypterCli(CLI::App* app, const char* argv0)
		: input{}, output{ "output"sv }
		, jobs{ 2 }, suffix{ "bundle"s }, recursive{ false }, quiet{ false }
		, PROGRAM_DIR{ std::filesystem::path{ argv0 }.parent_path() }
		, SEARCH_PATHS{
			std::filesystem::path{ "."sv },
			PROGRAM_DIR,
			PROGRAM_DIR / ".."sv
		}
		, synced_cout{}
	{
		app->add_option("input"s, input)
			->description("Input file or directory, if not specified, default search paths are used"s)
			->check(CLI::ExistingDirectory | CLI::ExistingFile)
			->take_all();
	
		app->add_option("-o,--output"s, output)
			->description("Output directory"s)
			->check(CLI::ExistingDirectory | CLI::NonexistentPath)
			->default_val(output.string());
		app->add_option("-j,--jobs"s, jobs)
			->description("Number of jobs"s)
			->default_val(2);
		app->add_option("-s,--suffix"s, suffix)
			->description("Suffix of asset bundle files, only works if input is a directory"s)
			->default_val(suffix);
		app->add_flag("-r,--recursive"s, recursive)
			->description("Recursively search input directories"s);
		app->add_flag("-q,--quiet"s, quiet)
			->description("Supress console output"s);


		std::string footer{ "Default Search Paths:\n"s };
		for (auto& s : SEARCH_PATHS)
		{
			std::format_to(std::back_inserter(footer),
				"  {}\n", (s / FOLDER_NAME).generic_string());
		}
		app->footer(footer);

		app->callback([this]() {
			this->resolve_input();
			this->execute();
		});
	}

	void decrpyt_file(const std::filesystem::path& file, int idx)
	{
		namespace fs = std::filesystem;
		if (!fs::is_regular_file(file))
			return;

		fs::path output_file;
		if (idx < 0)
			output_file.assign(output / file.filename());
		else
		{
			output_file.assign(
				output / input[idx].filename() / fs::relative(file, input[idx])
			);

			if (auto parent = output_file.parent_path(); !fs::exists(parent))
				fs::create_directories(parent);
		}
		std::ifstream ifs{ file, std::ios::binary | std::ios::in };
		ifs.exceptions(std::ios::badbit | std::ios::failbit);

		ByteArray data;
		ifs >> data;

		decrypt_bytes(data);
		std::ofstream ofs{ output_file, std::ios::binary | std::ios::out };
		ofs.write((char*)data.data(), data.size());

		print("{} -> {}\n", file.filename().string(), output_file.generic_string());
	}

	PathArray input;
	std::filesystem::path output;
	std::size_t jobs;
	std::string suffix;
	bool recursive;
	bool quiet;

	BS::synced_stream synced_cout;

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

	struct collect_bundle_helper
	{
		const std::string& extension_match;
		bool recursive;

		void operator()(BundleArray& bundles, const std::filesystem::path& dir, int idx) const
		{
			namespace fs = std::filesystem;
			auto do_work = [this, &bundles, idx](auto directory_iterator) {
				for (auto& entry : directory_iterator)
				{
					if (!entry.is_regular_file())
						continue;
	
					auto path = entry.path();
					if (path.extension().string() != extension_match)
						continue;
					bundles.emplace_back(std::move(path), idx);
				}
			};
			if (this->recursive)
				do_work(fs::recursive_directory_iterator{ dir });
			else
				do_work(fs::directory_iterator{ dir });
		}
	};

	BundleArray collect_files()
	{
		print("Collecting files...\n");
		namespace fs = std::filesystem;
		const std::string extension_match{ std::format(".{}", suffix) };

		const collect_bundle_helper add_bundles{ extension_match, recursive };

		BundleArray bundles;
		if (input.empty())
		{
			fs::path default_path = get_default_path();
			add_bundles(bundles, default_path, 0);
			input.emplace_back(std::move(default_path));
		}

		int idx = -1;
		for (auto& path : input)
		{
			idx++;
			if (std::filesystem::is_regular_file(path))
			{
				bundles.emplace_back(path, -1);
				continue;
			}
			if (!std::filesystem::is_directory(path))
				continue;
	
			add_bundles(bundles, path, idx);
		}

		return bundles;
	}

	template<typename ...Args>
	void print(std::format_string<Args...> fmt, Args&& ...args)
	{
		if (!quiet)
			synced_cout.print( std::format(fmt, std::forward<Args>(args)...));
	}

	// remove duplicates and resolve into absolute path
	void resolve_input()
	{
		namespace fs = std::filesystem;
		std::unordered_set<fs::path> seen;
		for (auto it = input.begin(); it != input.end(); ++it)
		{
			auto& path = (*it = fs::canonical(*it));
			if (!seen.contains(path))
			{
				seen.emplace(path);
				continue;
			}

			if (it = input.erase(it); it == input.end())
				break;
		}
	}

	void execute();
};

void DecrypterCli::execute()
{
	struct DecryptFailure
	{
		std::filesystem::path file;
		std::string msg;
	};

	if (!std::filesystem::exists(output))
		std::filesystem::create_directories(output);

	DecrypterCli::BundleArray input_files = collect_files();

	auto task_handler = [this, &input_files](std::size_t start, std::size_t end) {
		std::vector<DecryptFailure> failures;
		for (std::size_t i = start; i < end; ++i)
		{
			const auto& [file, idx] = input_files[i];
			try
			{
				decrpyt_file(file, idx);
			}
			catch (const std::exception& e)
			{
				failures.emplace_back(file, e.what());
			}
		}
		return failures;
	};

	BS::thread_pool pool{ jobs * 2 };
	auto future = pool.submit_blocks(
		0, input_files.size(),
		task_handler,
		jobs
	);

	std::size_t failed = 0;
	for (auto& failures : future.get())
	{
		for (auto& [file, err] : failures)
		{
			util::print(std::cerr, "Decryption failed: {}: {}\n", file.generic_string(), err);
		}
		failed += failures.size();
	}

	print("Completed: {}/{} success.", input_files.size() - failed, input_files.size());
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