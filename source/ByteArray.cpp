#include "ByteArray.hpp"
#include <stdexcept>
#include <ranges>
#include <cctype>


static constexpr bool is_hex_str(const char* str, std::size_t len)
{
	constexpr auto is_hex_char = [](char ch) {
		return ('0' <= ch && ch <= '9') || ('a' <= ch && ch <= 'f') || ('A' <= ch && ch <= 'F');
	};
	return std::all_of(str, str + len, is_hex_char);
}

constexpr ByteArray ByteArray::xor_encrpyt(const ByteArray& key, ByteArray::xor_new_array_tag tag) const
{
	ByteArray ret = *this;
	ret.xor_encrpyt(key, xor_inplace);
	return ret;
}
constexpr ByteArray& ByteArray::xor_encrpyt(const ByteArray& key, ByteArray::xor_inplace_tag tag)
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
constexpr ByteArray  ByteArray::xor_decrpyt(const ByteArray& key, ByteArray::xor_new_array_tag tag) const
{
	return xor_encrpyt(key, tag);
}
constexpr ByteArray& ByteArray::xor_decrpyt(const ByteArray& key, ByteArray::xor_inplace_tag tag)
{
	return xor_encrpyt(key, tag);
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