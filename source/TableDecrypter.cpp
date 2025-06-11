#include "Commands.hpp"

#ifdef ENABLE_TABLE_DECRYPTION
#include "util.hpp"

#include "TextMap.pb.h"
#include <pb_decode.h>

struct Text
{
	std::int64_t Id;
	std::string Content;
};

bool decode_Content(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
	std::string* content = (std::string*)*arg;
	content->resize(stream->bytes_left);
	return pb_read(stream, (uint8_t*)content->data(), content->size());
}

bool decode_TextMap(pb_istream_t* stream, const pb_field_t* field, void** arg)
{
	std::vector<Text>* map = (std::vector<Text>*)*arg;
	std::string content;
	TextMap data = TextMap_init_zero;

	data.Content.funcs.decode = decode_Content;
	data.Content.arg = &content;

	// it's ok if this returns false, since some entries don't have Content field
	pb_decode(stream, TextMap_fields, &data);

	map->emplace_back(data.Id, std::move(content));
	return true;
}

struct char_fill
{
	char character;
	std::uint32_t width;

	char_fill operator*(std::uint32_t val) const
	{ return { character, width * val }; }
};
template<>
struct std::formatter<char_fill>
{
	template<typename ParseContext>
	constexpr auto parse(ParseContext& ctx) const { return ctx.begin(); }

	template<typename FormatContext>
	constexpr auto format(const char_fill& f, FormatContext& ctx) const
	{ return std::ranges::fill_n(ctx.out(), f.width, f.character); }
};

struct u8char_view
{
	std::string_view str;

	std::uint32_t to_codepoint() const
	{
		const std::uint8_t* bytes = std::bit_cast<const std::uint8_t*>(str.data());
		const std::size_t len = str.size();

		if (len >= 2 && (bytes[0] & 0xE0) == 0xC0) {
			return ((bytes[0] & 0x1F) << 6) |
				    (bytes[1] & 0x3F);
		}
		else if (len >= 3 && (bytes[0] & 0xF0) == 0xE0) {
			return ((bytes[0] & 0x0F) << 12) |
				   ((bytes[1] & 0x3F) <<  6) |
				    (bytes[2] & 0x3F);
		}
		else if (len >= 4 && (bytes[0] & 0xF8) == 0xF0) {
			return ((bytes[0] & 0x07) << 18) |
				   ((bytes[1] & 0x3F) << 12) |
				   ((bytes[2] & 0x3F) <<  6) |
				    (bytes[3] & 0x3F);
		}
		return 0;
	}

	constexpr operator std::uint8_t() const
	{ return static_cast<std::uint8_t>(str.data()[0]); }
};


struct u8string_view
{
	std::string_view str;

	struct iterator
	{
		const char* data;
		std::size_t offset;
		std::size_t size;

		iterator(const char* d)
			: data{ d }, offset{ 0 }
		{ this->get_size(); }
		iterator(const char* d, std::size_t off)
			: data{ d }, offset{ off }, size{ std::string_view::npos }
		{}

		void get_size()
		{
			const char* ptr = data + offset;

			size = std::string_view::npos;
			std::uint8_t byte = *ptr;

			struct utf8std {
				std::uint8_t mask;
				std::uint8_t result;
				std::uint8_t size;
			};
			static constexpr utf8std masks[] = {
				{ 0x80, 0x00, 1 },
				{ 0xE0, 0xC0, 2 },
				{ 0xF0, 0xE0, 3 },
				{ 0xF8, 0xF0, 4 }
			};

			for (auto [mask, result, char_size] : masks)
			{
				if ((byte & mask) == result)
				{
					size = char_size;
					break;
				}
			}

			if (size > 4)
				throw std::runtime_error{ std::format("invalid utf-8 byte: {:x}", byte) };

		}

		u8char_view operator*() const
		{ return { { data + offset, size } }; }

		bool operator==(iterator rhs) const
		{
			return data == rhs.data && offset == rhs.offset;
		}

		auto operator<=>(iterator rhs) const
		{
			return (data + offset) <=> (rhs.data + rhs.offset);
		}

		iterator& operator++()
		{
			offset += size;
			this->get_size();

			return *this;
		}
	};

	iterator begin() const { return iterator{ str.data() }; };
	iterator end()   const { return iterator{ str.data(), str.size() }; };
};

struct ascii_escape_string{ std::string_view value; };

template<>
struct std::formatter<ascii_escape_string>
{
	constexpr auto parse(std::format_parse_context& ctx) const
	{ return ctx.begin(); }

	auto format(ascii_escape_string s, std::format_context& ctx) const
	{
		auto out = ctx.out();
		auto end = s.value.end();

#define CHAR_CASE(ch, replace) \
		case ch: \
			out = std::ranges::copy_n(replace, 2, out).out; \
			break

		for (auto it = s.value.begin(); it != end; ++it, ++out)
		{
			char ch = *it;
			switch (ch)
			{
			CHAR_CASE('\n', "\\n");
			CHAR_CASE('\t', "\\t");
			CHAR_CASE('\b', "\\b");
			CHAR_CASE('\r', "\\r");
			// CHAR_CASE('\'', "\\\'");
			CHAR_CASE('\"', "\\\"");
			default:
				*out = ch;
			};
		}
#undef CHAR_CASE
		return out;
	}
};

template<>
struct std::formatter<u8char_view>
{
	bool unicode_escape = false;
	bool ascii_escape   = false; // escape characters like \n, \t, etc

	std::formatter<ascii_escape_string> ascii_escape_formatter{};

	constexpr auto parse(std::format_parse_context& ctx)
	{
		std::string_view sv(ctx.begin(), ctx.end());
		unicode_escape = sv.find('u') != std::string_view::npos;
		ascii_escape   = sv.find('n') != std::string_view::npos;
		auto it = ctx.begin();
		while (*it != '}') ++it;
		return it;
	}

	auto format(const u8char_view& view, std::format_context& ctx) const
	{
		if (unicode_escape)
		{
			auto size = view.str.size();
			if (size <= 1)
				return ascii_escape
					? ascii_escape_formatter.format({ view.str }, ctx)
					: std::ranges::fill_n(ctx.out(), size, *view.str.data());

			return std::format_to(ctx.out(), "\\u{:0{}x}", view.to_codepoint(), 4 + ((size > 3) * 4));
		}
		if (ascii_escape)
			return ascii_escape_formatter.format(ascii_escape_string{ view.str }, ctx);

		return std::ranges::copy(view.str, ctx.out()).out;
	}
};

template<>
struct std::formatter<u8string_view>
{
	std::formatter<u8char_view> value_formatter{};
	constexpr auto parse(std::format_parse_context& ctx)
	{ return value_formatter.parse(ctx); }

	auto format(const ::u8string_view& sv, std::format_context& ctx) const
	{
		auto out = ctx.out();
		for (auto ch : sv)
			out = value_formatter.format(ch, ctx);
		return out;
	}
};
struct formatted_writer
{
	bool ensure_ascii = false;
	char_fill indent = { ' ', 4 };
	formatted_writer(bool ascii, char_fill indent)
		: ensure_ascii{ ascii }, indent{ indent } {}

	virtual ~formatted_writer() = default;

	virtual void operator()(std::ofstream& os, const std::vector<Text>& map) const
	{
		if (ensure_ascii)
		{
			out_ascii(os, map);
			return;
		}
		for (auto& text : map)
			util::print(os, "{} {:n}\n", text.Id, ::u8string_view{ text.Content });
	};
	virtual std::string_view extension() const { return "txt"; }

	void out_ascii(std::ofstream& os, const std::vector<Text>& map) const
	{
		for (auto& text : map)
			util::print(os, "{} {:un}\n", text.Id, ::u8string_view{ text.Content });
	}
};

struct json_writer : public formatted_writer
{
	using formatted_writer::formatted_writer;

	void operator()(std::ofstream& os, const std::vector<Text>& map) const override
	{
		util::print(os, "[");
		ensure_ascii ? out_ascii(os, map) : out_normal(os, map);
		util::print(os, "\n]\n");
	}
	virtual std::string_view extension() const override { return "json"; }

	void out_ascii(std::ofstream& os, const std::vector<Text>& map) const
	{
		std::string_view sep{ "\n" };
		const auto indent_2 = indent * 2;
		for (auto& text : map)
		{
			util::print(os,
				"{4}"
				"{0}{{"                      "\n"
				"{1}\"Id\": {2},"            "\n"
				"{1}\"Content\": \"{3:un}\""  "\n"
				"{0}}}"
				, indent, indent_2
				, text.Id, ::u8string_view{ text.Content }
				, sep
			);
			sep = ",\n";
		}
	}
	void out_normal(std::ofstream& os, const std::vector<Text>& map) const
	{
		std::string_view sep{ "\n" };
		const auto indent_2 = indent * 2;
		for (auto& text : map)
		{
			util::print(os,
				"{4}"
				"{0}{{"                      "\n"
				"{1}\"Id\": {2},"            "\n"
				"{1}\"Content\": \"{3:n}\""  "\n"
				"{0}}}"
				, indent, indent_2
				, text.Id, ::u8string_view{ text.Content }
				, sep
			);
			sep = ",\n";
		}
	}
};

struct json_kv_writer : public json_writer
{
	using json_writer::json_writer;

	void operator()(std::ofstream& os, const std::vector<Text>& map) const override
	{
		util::print(os, "{{");
		ensure_ascii ? out_ascii(os, map) : out_normal(os, map);
		util::print(os, "\n}}\n");
	}
	void out_ascii(std::ofstream& os, const std::vector<Text>& map) const
	{
		std::string_view sep{ "\n" };
		for (auto& text : map)
		{
			util::print(os,
				"{3}"
				"{0}\"{1}\": \"{2:un}\""
				, indent
				, text.Id, ::u8string_view{ text.Content }
				, sep
			);
			sep = ",\n";
		}
	}
	void out_normal(std::ofstream& os, const std::vector<Text>& map) const
	{
		std::string_view sep{ "\n" };
		for (auto& text : map)
		{
			util::print(os,
				"{3}"
				"{0}\"{1}\": \"{2:n}\""
				, indent
				, text.Id, ::u8string_view{ text.Content }
				, sep
			);
			sep = ",\n";
		}
	}
};
#else
struct formatted_writer{};
#endif

#include <charconv>

using namespace std::string_literals;
using namespace std::string_view_literals;

#define FORMAT_PLAIN   "plain"
#define FORMAT_JSON    "json"
#define FORMAT_JSON_KV "json:kv-pair"

const std::vector<std::string> TableDecrypter::format_choices{
	FORMAT_PLAIN,
	FORMAT_JSON,
	FORMAT_JSON_KV
};


struct IndentValidator : public CLI::Validator
{

	IndentValidator()
		: CLI::Validator{ "UINT OR Literal('t')" }
	{
		func_ = [](const std::string& input)
		{
			if (input.size() == 1 && input[0] == 't')
				return ""s;

			if (std::ranges::all_of(input, [](char ch) { return std::isdigit(ch); }) &&
				std::ranges::any_of(input, [](char ch) { return ch != '0'; }))
				return ""s;

			return std::format("Expect a positive integer or 't', got: {}", input);
		};
	}

	static char_fill parse_input(const std::string& input)
	{
		char_fill ret{ ' ', 1 };
		if (input.size() == 0 && input[0] == 't')
			ret.character = '\t';
		else
			ret.width = std::stoi(input);
		return ret;
	}
};

TableDecrypter::TableDecrypter(CLI::App* app, const char* argv0)
	: Command{ app, argv0 }
{
	app->add_option("input"s, input)
		->description("Input files"s)
		->check(CLI::ExistingFile)
		->required(true)
		->take_all();

	app->add_option("-o,--output"s, output_dir)
		->description("Output directory"s)
		->check(CLI::ExistingDirectory | CLI::NonexistentPath)
		->default_val(output_dir.string());

	app->add_flag("-a,--ascii"s, ensure_ascii)
		->description("Ensure ascii in output files");

	app->add_option("-i,--indent"s, indent)
		->check(IndentValidator{})
		->description("Set indentation size, it is ignored if output format is not json"s);


	app->add_flag("-s,--sort"s, sort)
		->default_val(false)
		->description("Sort content by id"s);

	app->add_option("-F,--format", format)
		->default_val("json")
		->description("Output file format")
		->check(CLI::IsMember(format_choices, CLI::ignore_case));
}

TableDecrypter::~TableDecrypter() {}


void TableDecrypter::operator()()
{
#ifdef ENABLE_TABLE_DECRYPTION
	char_fill indent_ch = IndentValidator::parse_input(indent);

	if (format == FORMAT_JSON)
		output_writer.reset(new json_writer{ ensure_ascii, indent_ch});
	else if (format == FORMAT_JSON_KV)
		output_writer.reset(new json_kv_writer{ ensure_ascii, indent_ch });
	else
		output_writer.reset(new formatted_writer{ ensure_ascii, indent_ch });

	this->execute();
#else
	quiet = false;
	throw std::runtime_error{ "Table decryption feature is not enabled for this build." };
#endif
}

#ifdef ENABLE_TABLE_DECRYPTION
void TableDecrypter::decode_file(const File& f)
{
	TextMapTable table = TextMapTable_init_zero;

	std::vector<uint8_t> buffer;
	{
		std::ifstream ifs{ f.source, std::ios::binary | std::ios::in | std::ios::ate };
		if (!ifs.is_open())
			throw std::runtime_error{ std::format("cannot open: {}", f.source.generic_string()) };
		buffer.resize(ifs.tellg());
		ifs.seekg(0, std::ios::beg);
		ifs.read((char*)buffer.data(), buffer.size());
	}

	std::vector<Text> data;

	table.Data.funcs.decode = decode_TextMap;
	table.Data.arg = &data;

	pb_istream_t stream = pb_istream_from_buffer(buffer.data(), buffer.size());
	if (!pb_decode(&stream, TextMapTable_fields, &table))
	{
		throw std::runtime_error{ PB_GET_ERROR(&stream) };
	}

	if (sort)
	{
		std::ranges::sort(data, [](const Text& lhs, const Text& rhs) {
			return lhs.Id < rhs.Id;
		});
	}

	auto target = f.target;

	if (!dry_run)
	{
		std::ofstream ofs{
			target.replace_extension(output_writer->extension()),
			std::ios::out | std::ios::trunc
		};
		(*output_writer)(ofs, data);
	}
	print("{} -> {}\n", f.source.generic_string(), target.generic_string());
}

void TableDecrypter::execute()
{

	std::vector<File> input_files;
	{
		std::set<File> file_set = collect_files(input, output_dir);
		input_files.reserve(file_set.size());
		std::ranges::copy(
			std::make_move_iterator(file_set.begin()),
			std::make_move_iterator(file_set.end()),
			std::back_inserter(input_files)
		);
	}

	print("Total: {} files\n", input_files.size());

	struct DecodeFailure
	{
		std::filesystem::path file;
		std::string msg;
	};

	auto worker = [&input_files, this](std::size_t begin, std::size_t end) {
		std::vector<DecodeFailure> failures;

		for (std::size_t i = begin; i < end; ++i)
		{
			decode_file(input_files[i]);
		}
		return failures;
	};

	BS::thread_pool pool{ 0 };
	auto future = pool.submit_blocks(0, input_files.size(), worker);

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
#endif

