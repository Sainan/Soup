#include "cli.hpp"

#include <cstring> // memcmp

#include <aes.hpp>
#include <Benchmark.hpp>
#include <rand.hpp>

void cli_bench()
{
	BENCHMARK("AES-ECB-128", {
		uint8_t og_data[0x10'000];
		soup::rand.fill(og_data);
		uint8_t data[sizeof(og_data)];
		const char key[] = "Super Secret Key";
		BENCHMARK_LOOP({
			memcpy(data, og_data, sizeof(data));
			soup::aes::ecbEncrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 16);
			soup::aes::ecbDecrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 16);
			SOUP_ASSERT(memcmp(data, og_data, sizeof(data)) == 0);
		});
	});
	BENCHMARK("AES-ECB-192", {
		uint8_t og_data[0x10'000];
		soup::rand.fill(og_data);
		uint8_t data[sizeof(og_data)];
		const char key[] = "Super Secret 192-Bit Key";
		BENCHMARK_LOOP({
			memcpy(data, og_data, sizeof(data));
			soup::aes::ecbEncrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 24);
			soup::aes::ecbDecrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 24);
			SOUP_ASSERT(memcmp(data, og_data, sizeof(data)) == 0);
		});
	});
	BENCHMARK("AES-ECB-256", {
		uint8_t og_data[0x10'000];
		soup::rand.fill(og_data);
		uint8_t data[sizeof(og_data)];
		const char key[] = "My Super Secret Key For 256-Bit";
		BENCHMARK_LOOP({
			memcpy(data, og_data, sizeof(data));
			soup::aes::ecbEncrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 32);
			soup::aes::ecbDecrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 32);
			SOUP_ASSERT(memcmp(data, og_data, sizeof(data)) == 0);
		});
	});
	BENCHMARK("AES-CBC-128", {
		uint8_t og_data[0x10'000];
		soup::rand.fill(og_data);
		uint8_t data[sizeof(og_data)];
		const char key[] = "Super Secret Key";
		const char iv[] = "Super Secret IV";
		BENCHMARK_LOOP({
			memcpy(data, og_data, sizeof(data));
			soup::aes::cbcEncrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 16, reinterpret_cast<const uint8_t*>(iv));
			soup::aes::cbcDecrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 16, reinterpret_cast<const uint8_t*>(iv));
			SOUP_ASSERT(memcmp(data, og_data, sizeof(data)) == 0);
		});
	});
	BENCHMARK("AES-CBC-192", {
		uint8_t og_data[0x10'000];
		soup::rand.fill(og_data);
		uint8_t data[sizeof(og_data)];
		const char key[] = "Super Secret 192-Bit Key";
		const char iv[] = "Super Secret IV";
		BENCHMARK_LOOP({
			memcpy(data, og_data, sizeof(data));
			soup::aes::cbcEncrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 24, reinterpret_cast<const uint8_t*>(iv));
			soup::aes::cbcDecrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 24, reinterpret_cast<const uint8_t*>(iv));
			SOUP_ASSERT(memcmp(data, og_data, sizeof(data)) == 0);
		});
	});
	BENCHMARK("AES-CBC-256", {
		uint8_t og_data[0x10'000];
		soup::rand.fill(og_data);
		uint8_t data[sizeof(og_data)];
		const char key[] = "My Super Secret Key For 256-Bit";
		const char iv[] = "Super Secret IV";
		BENCHMARK_LOOP({
			memcpy(data, og_data, sizeof(data));
			soup::aes::cbcEncrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 32, reinterpret_cast<const uint8_t*>(iv));
			soup::aes::cbcDecrypt(data, sizeof(data), reinterpret_cast<const uint8_t*>(key), 32, reinterpret_cast<const uint8_t*>(iv));
			SOUP_ASSERT(memcmp(data, og_data, sizeof(data)) == 0);
		});
	});
}
