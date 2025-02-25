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
	public:  constexpr static name##_tag name

	DECLARE_TAG(xor_new_array);
	DECLARE_TAG(xor_inplace);

#undef DECLARE_TAG
public:
	template<typename T>
		requires std::is_convertible_v<T, std::underlying_type_t<std::byte>>
	constexpr ByteArray(std::initializer_list<T> intializer)
	{
		this->reserve(intializer.size());
		std::ranges::transform(
			intializer.begin(), intializer.end(), std::back_inserter(*this),
			[](T a) { return (std::byte)a; }
		);
	}
	using std::vector<std::byte>::vector;
	
	ByteArray xor_encrpyt(const ByteArray& key, xor_new_array_tag tag = xor_new_array) const
	{
		ByteArray ret = *this;
		ret.xor_encrpyt(key, xor_inplace);
		return ret;
	}
	ByteArray& xor_encrpyt(const ByteArray& key, xor_inplace_tag tag)
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

public:
	ByteArray operator^(const ByteArray& other) const
	{
		if (this == &other)
			return ByteArray(this->size(), std::byte{});

		ByteArray ret(std::min(this->size(), other.size()));

		std::ranges::transform(
			this->begin(), this->end(),
			other.begin(), other.end(),
			ret.begin(),
			std::bit_xor<std::byte>{}
		);
		return ret;
	}
	ByteArray& operator^=(const ByteArray& other)
	{
		if (this == &other)
		{
			std::fill(this->begin(), this->end(), std::byte{});
			return *this;
		}

		this->resize(std::min(this->size(), other.size()));
		std::ranges::transform(
			this->begin(), this->end(),
			other.begin(), other.end(),
			this->begin(),
			std::bit_xor<std::byte>{}
		);
		return *this;
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

inline constexpr bool is_hex_str(const char* str, std::size_t len)
{
	constexpr auto is_hex_char = [](char ch) {
		return ('0' <= ch && ch <= '9') || ('a' <= ch && ch <= 'f') || ('A' <= ch && ch <= 'F');
	};
	return std::all_of(str, str + len, is_hex_char);
}

inline constexpr ByteArray operator""_hex(const char* str, std::size_t len)
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
	const std::byte* ptr = (const std::byte*)str;
	return ByteArray{ ptr, ptr + len };
}