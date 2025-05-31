#include "Decrypter.hpp"

const ByteArray Decrypter::DECRYPTION_KEY =
	"\x55\x6E\x69\x74\x79\x46\x53\x00\x00\x00\x00\x07\x35\x2E\x78\x2E"_bytes;

ByteArray& Decrypter::decrypt_bytes(ByteArray& data)
{
	auto key = data ^ DECRYPTION_KEY;
	return data.xor_encrpyt(key, 0x1000 * 8, ByteArray::xor_inplace);
}
