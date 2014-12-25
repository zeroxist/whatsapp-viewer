#include <fstream>
#include <iostream>
#include <vector>

#include "../Exceptions/Exception.h"
#include "../Libraries/AES/AES.h"
#include "../Libraries/Zip/zlib.h"
#include "Crypt5.h"
#include "Crypt7.h"

const int chunk = 16384;

void uncompressBlock(z_stream &stream, std::vector<unsigned char> &uncompressed)
{
	unsigned char out[chunk];

	do
	{
		stream.avail_out = chunk;
		stream.next_out = out;
		int ret = inflate(&stream, Z_NO_FLUSH);

		if (ret != Z_OK && ret != Z_STREAM_END)
		{
			inflateEnd(&stream);
			throw Exception("Decryption failed. Error during unzipping (inflate).");
		}

		unsigned int have = chunk - stream.avail_out;

		for (unsigned int i = 0; i < have; i++)
		{
			uncompressed.push_back(out[i]);
		}
	} while (stream.avail_out == 0);
}

void uncompressGzipBuffer(unsigned char *compressedBytes, int size, std::vector<unsigned char> &uncompressed)
{
	unsigned char in[chunk];

	z_stream stream;
	memset(&stream, 0, sizeof(z_stream));

	int ret = inflateInit2(&stream, 16 + MAX_WBITS);
	if (ret != Z_OK)
	{
		throw Exception("Decryption failed. Error during unzipping (inflateInit).");
	}

	unsigned char *currentPosition = compressedBytes;

	do
	{
		int bytesLeft = size - (currentPosition - compressedBytes);
		if (bytesLeft == 0)
		{
            break;
		}

		int bytesToCopy = bytesLeft;
		if (bytesLeft > chunk)
		{
			bytesToCopy = chunk;
		}

		memcpy(in, currentPosition, bytesToCopy);
        stream.avail_in = bytesToCopy;
        stream.next_in = in;

		uncompressBlock(stream, uncompressed);

		currentPosition += bytesToCopy;
	} while (ret != Z_STREAM_END);

	inflateEnd(&stream);
}

void decryptWhatsappDatabase8(const std::string &filename, const std::string &filenameDecrypted, unsigned char *key, unsigned char *initVector)
{
	unsigned char *fileBytes;
	int filesize = loadFileUnsigned(filename, &fileBytes);

	unsigned char iv[16];
	memcpy(iv, initVector, 16);

	const int skipBytes = 67;
	filesize -= skipBytes;
	unsigned char *databaseBytes = &fileBytes[skipBytes];

	decrypt_aes_cbc_256(databaseBytes, databaseBytes, filesize, key, iv);

	std::vector<unsigned char> uncompressed;
	uncompressGzipBuffer(databaseBytes, filesize, uncompressed);

	const char expectedBytes[] = "SQLite format 3";
	if (memcmp(&uncompressed[0], expectedBytes, sizeof(expectedBytes)) != 0)
	{
		throw Exception("Decryption failed. Invalid key?");
	}

	std::ofstream output(filenameDecrypted.c_str(), std::ios::binary);
	if (!output)
	{
		throw Exception("Could not save decrypted WhatsApp database. Permissions needed?");
	}

	output.write(reinterpret_cast<char *>(&uncompressed[0]), uncompressed.size());

	delete[] fileBytes;
}

void decryptWhatsappDatabase8(const std::string &filename, const std::string &filenameDecrypted, const std::string &keyFilename)
{
	unsigned char key[32];
	unsigned char iv[16];

	extractKey(keyFilename, key, iv);

	decryptWhatsappDatabase8(filename, filenameDecrypted, key, iv);
}
