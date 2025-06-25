#include "Commands.hpp"
#include "util.hpp"

#include <string_view>
#include <filesystem>
#include <unordered_set>

using namespace std::string_literals;
using namespace std::string_view_literals;

const ByteArray BundleDecrypter::DECRYPTION_KEY =
	"\x55\x6E\x69\x74\x79\x46\x53\x00\x00\x00\x00\x07\x35\x2E\x78\x2E"_bytes;

ByteArray& BundleDecrypter::decrypt_bytes(ByteArray& data)
{
	auto key = data ^ DECRYPTION_KEY;
	return data.xor_encrpyt(key, 0x1000 * 8, ByteArray::xor_inplace);
}

BundleDecrypter::BundleDecrypter(CLI::App* app, const char* argv0)
	: Command{ app, argv0 }
	, input{}, output{ "output"sv }
	, jobs{ 2 }, suffix{ "bundle"s }
	, SEARCH_PATHS{
		std::filesystem::path{ "."sv },
		PROGRAM_DIR,
		PROGRAM_DIR / ".."sv
	}
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
		->description("Number of jobs, if 0 is provided, it is automatically determined"s)
		->default_val(0);
	app->add_option("-s,--suffix"s, suffix)
		->description("Suffix of asset bundle files, only works if input is a directory"s)
		->default_val(suffix);
	app->add_flag("-r,--recursive"s, recursive)
		->description("Recursively search input directories"s);



	std::string footer{ "Default Search Paths:\n"s };
	for (auto& s : SEARCH_PATHS)
	{
		std::format_to(std::back_inserter(footer),
			"  {}\n", (s / FOLDER_NAME).generic_string());
	}
	app->footer(footer);
}

void BundleDecrypter::operator()()
{
	this->resolve_input();
	suffix = std::move(std::format(".{}", suffix));

	this->execute();
}

std::filesystem::path BundleDecrypter::get_default_path()
{
	for (auto& search : SEARCH_PATHS)
	{
		auto p = search / FOLDER_NAME;
		if (std::filesystem::is_directory(p))
			return std::filesystem::canonical(p);
	}
	throw std::runtime_error{ "No valid asset bundle folder found in default search path, please specify input" };
}

void BundleDecrypter::resolve_input()
{
	namespace fs = std::filesystem;

	if (input.empty())
		input.emplace_back(get_default_path());
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

void BundleDecrypter::decrypt_file(const File& f)
{
	ByteArray data;

	std::ifstream ifs{ f.source, std::ios::binary | std::ios::in | std::ios::ate };
	std::size_t file_size = ifs.tellg();
	std::size_t read_size = std::min<std::size_t>(0x1000 * 8, file_size); // only read what we need
	data.resize(read_size);

	ifs.seekg(0, std::ios::beg);
	ifs.read((char*)data.data(), read_size);

	decrypt_bytes(data);

	if (!dry_run)
	{
		std::filesystem::create_directories(f.target.parent_path());
		std::ofstream ofs{ f.target, std::ios::binary | std::ios::out };
		ofs.write((char*)data.data(), data.size());
		if (file_size > read_size)
			ofs << ifs.rdbuf();
	}

	print("{} -> {}\n", f.source.filename().string(), f.target.generic_string());
}

void BundleDecrypter::execute()
{
	struct DecryptFailure
	{
		std::filesystem::path file;
		std::string msg;
	};

	if (!dry_run && !std::filesystem::exists(output))
		std::filesystem::create_directories(output);

	print("Collecting files...");
	std::set<File> file_set = collect_files(input, output);
	std::vector<File> input_files;
	input_files.reserve(file_set.size());
	std::ranges::copy(
		std::make_move_iterator(file_set.begin()),
		std::make_move_iterator(file_set.end()),
		std::back_inserter(input_files)
	);
	print("\b\b\b: found {} files\n", input_files.size());

	auto task_handler = [this, &input_files](std::size_t start, std::size_t end) {
		std::vector<DecryptFailure> failures;
		for (std::size_t i = start; i < end; ++i)
		{
			try
			{
				decrypt_file(input_files[i]);
			}
			catch (const std::exception& e)
			{
				failures.emplace_back(input_files[i].source, e.what());
			}
		}
		return failures;
	};

	BS::thread_pool pool{ std::max<std::size_t>(jobs, 0) };
	auto future = pool.submit_blocks(
		0, input_files.size(),
		task_handler,
		jobs * 2
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

	print("Completed: {}/{} success.\n", input_files.size() - failed, input_files.size());
}

