#include "Commands.hpp"

Command::File::File(
	const std::filesystem::path& src,
	const std::filesystem::path& output,
	std::optional<std::filesystem::path> root
) : source{ src }
{
	if (!std::filesystem::is_regular_file(src))
		throw std::invalid_argument{ "source is not a file" };
	if (std::filesystem::exists(output) && !std::filesystem::is_directory(output))
		throw std::invalid_argument{
			std::format("output is not a directory: {}", output.generic_string())
		};

	if (!root)
	{
		const_cast<std::filesystem::path&>(target) = std::move(output / src.filename());
		return;
	}
	auto& src_root = root.value();
	if (!std::filesystem::is_directory(src_root))
		throw std::invalid_argument{ "root is not a directory" };

	auto tgt = src.lexically_relative(src_root.parent_path());
	if (tgt.empty())
		throw std::invalid_argument{ "root is not a lexical parent of source" };

	const_cast<std::filesystem::path&>(target) = std::move(output / tgt);
}

std::set<Command::File> Command::collect_files(
	const std::vector<std::filesystem::path>& input,
	const std::filesystem::path& output_dir)
{
	std::set<File> ret;

	for (auto& p : input)
	{
		if (std::filesystem::is_regular_file(p))
		{
			ret.emplace(p, output_dir);
			continue;
		}
		if (!std::filesystem::is_directory(p))
			continue;

		if (recursive)
		{
			traverse_directory(
				ret,
				std::filesystem::recursive_directory_iterator{ p },
				p, output_dir
			);
		}
		else
		{
			traverse_directory(
				ret,
				std::filesystem::directory_iterator{ p },
				p, output_dir
			);
		}
	}

	return ret;
}