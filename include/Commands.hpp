#pragma once

#include <CLI/CLI.hpp>
#include <BS_thread_pool.hpp>

#include "ByteArray.hpp"

class Command
{
public:
	struct File
	{
		const std::filesystem::path source;
		const std::filesystem::path target;

		auto operator<=>(const File& other) const
		{  return source <=> other.source; }
	// private:
		File(
			const std::filesystem::path& src,
			const std::filesystem::path& output,
			std::optional<std::filesystem::path> root = std::nullopt
		);
		friend class Command;
	};
public:
	Command(CLI::App* app, const char* argv0)
		: app{ app }
		, PROGRAM_DIR{ std::filesystem::path{ argv0 }.parent_path() }
		, recursive{ false }, quiet{ false }, dry_run{ false }
		, synced_cout{}
	{ app->callback([this]() { this->operator()(); }); }

	virtual void operator()() = 0;

	template<typename ...Args>
	void print(std::format_string<Args...> fmt, Args&&... args) const
	{
		if (quiet) return;
		synced_cout.print(std::format(fmt, std::forward<Args>(args)...));
	}

	std::set<File> collect_files(
		const std::vector<std::filesystem::path>& input,
		const std::filesystem::path& output_dir
	);

protected:
	CLI::App* app;
	const std::filesystem::path PROGRAM_DIR;

	bool recursive;
	bool quiet;
	bool dry_run;

	mutable BS::synced_stream synced_cout;

	virtual bool match_file(const std::filesystem::path& file) const { return true; }
private:
	template<typename Iter>
	void traverse_directory(
		std::set<File>& files, Iter directory_iterator,
		const std::filesystem::path& dir,
		const std::filesystem::path& output_dir)
	{
		for (auto& entry : directory_iterator)
		{
			if (!entry.is_regular_file())
				continue;

			auto path = entry.path();
			if (!match_file(path))
				continue;
			files.emplace(std::move(path), output_dir, dir);
		}
	}
};

class BundleDecrypter : public Command
{
public:
	using PathArray = std::vector<std::filesystem::path>;
	using PathSet   = std::set<std::filesystem::path>;

	BundleDecrypter(CLI::App* app, const char* argv0);

	virtual void operator()() override;

	// remove duplicates and resolve into absolute path
	void resolve_input();
	void execute();

protected:
	virtual bool match_file(const std::filesystem::path& p) const override
	{ return p.extension() == suffix; }

private:
	PathArray input;
	std::filesystem::path output;
	std::size_t jobs;
	std::string suffix;


	const std::filesystem::path SEARCH_PATHS[3];
	static constexpr std::string_view FOLDER_NAME{ "AssetBundles_Windows" };

	static const ByteArray DECRYPTION_KEY;
	static ByteArray& decrypt_bytes(ByteArray& data);

	void decrypt_file(const File& f);
	std::filesystem::path get_default_path();
};

