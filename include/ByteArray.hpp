#pragma once

#include <functional>
#include <algorithm>
#include <fstream>
#include <vector>
#include <ranges>

class ByteArray : public std::vector<std::byte>
{
#define DECLARE_TAG(name) \
	private: struct name##_tag{};  \
	public:  constexpr static name##_tag name{}

	DECLARE_TAG(xor_new_array);
	DECLARE_TAG(xor_inplace);

#undef DECLARE_TAG
public:
	using underlying_value_type = std::underlying_type_t<std::byte>;

	template<typename T>
		requires std::is_convertible_v<T, underlying_value_type>
	constexpr ByteArray(std::initializer_list<T> intializer)
	{
		this->reserve(intializer.size());
		std::ranges::transform(
			intializer.begin(), intializer.end(), std::back_inserter(*this),
			[](T a) { return (std::byte)a; }
		);
	}
	using std::vector<std::byte>::vector;

	constexpr ByteArray  xor_encrpyt(const ByteArray& key, xor_new_array_tag tag = xor_new_array) const
	{
		ByteArray ret = *this;
		ret.xor_encrpyt(key, xor_inplace);
		return ret;
	}
	constexpr ByteArray& xor_encrpyt(const ByteArray& key, xor_inplace_tag tag)
	{
		std::size_t key_size = key.size();
		auto key_view = std::views::iota((std::size_t)0, key_size)
			| std::views::transform([key_size, &key](size_t i) {
				return key[i % key_size];
			});

		std::ranges::transform(
			*this, key_view,
			this->begin(),
			std::bit_xor<std::byte>{}
		);
		return *this;
	}

	constexpr ByteArray  xor_decrypt(const ByteArray& key, xor_new_array_tag tag = xor_new_array) const
	{ return xor_encrpyt(key, tag); }
	constexpr ByteArray& xor_decrypt(const ByteArray& key, xor_inplace_tag tag)
	{ return xor_encrpyt(key, tag); }

	template<typename CharT>
		requires (sizeof(CharT) == sizeof(std::byte))
	constexpr static ByteArray from_bytes(std::basic_string_view<CharT> bytes)
	{
		const std::byte* ptr = std::bit_cast<const std::byte*>(bytes.data());
		return { ptr, ptr + bytes.size() };
	}
	template<typename CharT, std::size_t N>
		requires (sizeof(CharT) == sizeof(std::byte))
	constexpr static ByteArray from_bytes(const CharT(& bytes)[N])
	{
		const std::byte* ptr = std::bit_cast<const std::byte*>(bytes);
		return { ptr, ptr + N - 1 };
	}

	template<std::integral IntT>
	constexpr static ByteArray from_integer(IntT integer)
	{
		const std::byte* byte_ptr = std::bit_cast<const std::byte*>(&integer);
		return { byte_ptr, byte_ptr + sizeof(integer) };
	}

	template<std::ranges::range RangeT>
	constexpr static ByteArray from_integers(RangeT&& rng)
	{
		using rng_value_t = std::ranges::range_value_t<RangeT>;

		auto bytes_view = rng | std::views::transform([](const rng_value_t& v) {
			return std::as_bytes(std::span<rng_value_t, 1>{ &v, 1 });
		}) | std::views::join;

		ByteArray ret;
		return ret.copy_from_range(bytes_view);
	}
	template<std::integral IntT>
	constexpr static ByteArray from_integers(std::initializer_list<IntT> rng)
	{
		auto bytes_view = rng | std::views::transform([](const IntT& v) {
			return std::as_bytes(std::span<IntT, 1>{ &v, 1 });
		}) | std::views::join;

		ByteArray ret;
		return ret.copy_from_range(bytes_view);
	}

	template<typename T>
		requires std::is_trivially_copyable_v<T>
	constexpr static ByteArray from_trivially_copyable(const T& value)
	{
		const std::byte* byte_ptr = std::bit_cast<const std::byte*>(&value);
		return { byte_ptr, byte_ptr + sizeof(T) };
	}

private:
	template<typename BinaryOp>
		requires std::is_invocable_r_v<std::byte, BinaryOp, std::byte, std::byte>
	constexpr ByteArray& bit_operate(const ByteArray& other, ByteArray& result, BinaryOp op) const
	{
		std::ranges::transform(
			this->begin(), this->end(),
			other.begin(), other.end(),
			result.begin(),
			op
		);
		return result;
	}

	template<std::ranges::range RangeT, typename Iterator>
	constexpr ByteArray& copy_from_range(RangeT&& range, Iterator where, std::size_t reserve = 0)
	{
		if (reserve > 0)
			this->reserve(reserve);
		std::ranges::copy(range, where, reserve);
		return *this;
	}


public:
	constexpr ByteArray operator^(const ByteArray& other) const
	{
		if (this == &other)
			return ByteArray(this->size(), std::byte{});

		ByteArray ret(std::min(this->size(), other.size()));
		return bit_operate(other, ret, std::bit_xor<std::byte>{});
	}
	constexpr ByteArray& operator^=(const ByteArray& other)
	{
		if (this == &other)
		{
			std::fill(this->begin(), this->end(), std::byte{});
			return *this;
		}

		this->resize(std::min(this->size(), other.size()));
		return bit_operate(other, *this, std::bit_xor<std::byte>{});
	}
#define bit_operator_define(op, name)                                   \
	constexpr ByteArray op(const ByteArray& other) const {              \
		if (this == &other)                                             \
			return other;                                               \
		ByteArray ret(std::min(this->size(), other.size()));            \
		return bit_operate(other, ret, std::bit_##name<std::byte>{});   \
	}                                                                   \
	constexpr ByteArray& op##=(const ByteArray& other) {                \
		if (this == &other)                                             \
			return *this;                                               \
		this->resize(std::min(this->size(), other.size()));             \
		return bit_operate(other, *this, std::bit_##name<std::byte>{}); \
	}

	bit_operator_define(operator&, and);
	bit_operator_define(operator|, or);

#undef bit_operator_define
	constexpr ByteArray operator~() const
	{
		ByteArray ret{ *this };
		std::ranges::transform(
			ret.begin(), ret.end(),
			ret.begin(),
			std::bit_not<std::byte>{}
		);
		return ret;
	}
};

inline std::ifstream& operator>>(std::ifstream& ifs, ByteArray& arr)
{
	const auto current_pos = ifs.tellg();
	if (current_pos == -1)
	{
		ifs.setstate(std::ios::failbit);
		return ifs;
	}

	ifs.seekg(0, std::ios::end);
	const auto end_pos = ifs.tellg();
	if (end_pos == -1)
	{
		ifs.seekg(current_pos);
		ifs.setstate(std::ios::failbit);
		return ifs;
	}

	const auto size = end_pos - current_pos;
	if (size < 0)
	{
		ifs.seekg(current_pos);
		ifs.setstate(std::ios::failbit);
		return ifs;
	}
	ifs.seekg(current_pos);

	arr.resize(size);
	ifs.read((char*)arr.data(), size);

	return ifs;
}

static constexpr bool is_hex_str(const char* str, std::size_t len)
{
	constexpr auto is_hex_char = [](char ch) {
		return ('0' <= ch && ch <= '9') || ('a' <= ch && ch <= 'f') || ('A' <= ch && ch <= 'F');
	};
	return std::all_of(str, str + len, is_hex_char);
}

constexpr ByteArray operator""_hex(const char* str, std::size_t len)
{
	if (!is_hex_str(str, len) || (len & 1) == 1)
		throw std::invalid_argument("invalid hex string");

	constexpr auto hex_char_to_value = [](char c) {
		c = std::tolower(static_cast<unsigned char>(c));
		if (c >= '0' && c <= '9')
			return (std::byte)(c - '0');
		else if (c >= 'a' && c <= 'f')
			return (std::byte)(10 + c - 'a');
		else
			throw std::invalid_argument("invalid hex character");
	};
	ByteArray result;
	result.reserve(len / 2);

	for (size_t i = 0; i < len; i += 2)
	{
		std::byte high = hex_char_to_value(str[i]);
		std::byte low  = hex_char_to_value(str[i + 1]);
		result.push_back((high << 4) | low);
	}
	return result;
}
inline constexpr ByteArray operator""_bytes(const char* str, std::size_t len)
{
	const std::byte* ptr = std::bit_cast<const std::byte*>(str);
	return ByteArray{ ptr, ptr + len };
}
