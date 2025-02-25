#include "Decrypter.hpp"
#include <ranges>

const ByteArray Decrypter::DECRYPTION_KEY =
	"\x55\x6E\x69\x74\x79\x46\x53\x00\x00\x00\x00\x07\x35\x2E\x78\x2E"_bytes;

ByteArray& Decrypter::decrypt_bytes(ByteArray& data)
{
	auto key = data ^ DECRYPTION_KEY;
	std::size_t key_len  = key.size();
	std::size_t n = std::min<std::size_t>(0x1000 * 8, data.size());

	auto key_view = std::views::iota((std::size_t)0, n)
		| std::views::transform([key_len, &key](size_t i) {
			return key[i % key_len];
		});

	std::ranges::transform(
		data | std::views::take(n),
		key_view,
		data.begin(),
		std::bit_xor<std::byte>{}
	);
	return data;
}