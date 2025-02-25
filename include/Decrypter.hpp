#pragma once

#include "ByteArray.hpp"

struct Decrypter
{
	static const ByteArray DECRYPTION_KEY;
	static ByteArray& decrypt_bytes(ByteArray& data);
};

