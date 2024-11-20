#include "CpuInfo.hpp"
#if !SOUP_WASM

#if SOUP_X86
	#if defined(_MSC_VER) && !defined(__clang__)
		#include <intrin.h>
	#else
		#include <cpuid.h>
	#endif
#elif SOUP_ARM
	#if SOUP_WINDOWS
		#include <windows.h>
	#elif !SOUP_MACOS
		#include <sys/auxv.h>
	#endif
#endif

NAMESPACE_SOUP
{
	CpuInfo::CpuInfo() noexcept
	{
#if SOUP_X86
	#define EAX arr[0]
	#define EBX arr[1]
	#define EDX arr[2]
	#define ECX arr[3]

		char buf[17];
		buf[16] = 0;
		invokeCpuid(buf, 0);
		cpuid_max_eax = *reinterpret_cast<uint32_t*>(&buf[0]);
		vendor_id = &buf[4];

		uint32_t arr[4];

		if (cpuid_max_eax >= 0x01)
		{
			invokeCpuid(arr, 0x01);
			stepping_id = (EAX & 0xF);
			model = ((EAX >> 4) & 0xF);
			family = ((EAX >> 8) & 0xF);
			feature_flags_ecx = ECX;
			feature_flags_edx = EDX;

			if (cpuid_max_eax >= 0x07)
			{
				invokeCpuid(arr, 0x07, 0);
				extended_features_max_ecx = EAX;
				extended_features_0_ebx = EBX;

				if (extended_features_max_ecx >= 1)
				{
					invokeCpuid(arr, 0x07, 1);
					extended_features_1_eax = EAX;
				}

				if (cpuid_max_eax >= 0x16)
				{
					invokeCpuid(arr, 0x16);
					base_frequency = EAX;
					max_frequency = EBX;
					bus_frequency = ECX;
				}
			}
		}

		invokeCpuid(arr, 0x80000000);
		cpuid_extended_max_eax = EAX;

		if (cpuid_extended_max_eax >= 0x80000001)
		{
			invokeCpuid(arr, 0x80000001);
			extended_flags_1_ecx = ECX;
		}

	#undef EAX
	#undef EBX
	#undef EDX
	#undef ECX
#elif SOUP_ARM
	#if SOUP_WINDOWS
		armv8_aes = IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
		armv8_sha1 = IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
		armv8_sha2 = IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
		armv8_crc32 = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
	#elif SOUP_BITS == 32
		armv8_aes = false;
		armv8_sha1 = false;
		armv8_sha2 = false;
		armv8_crc32 = false;
	#elif SOUP_MACOS
		// Assume we are running on an M series chip, because we don't have getauxval.
		armv8_aes = true;
		armv8_sha1 = true;
		armv8_sha2 = true;
		armv8_crc32 = true;
	#else
		// These HWCAP_* are only defined on aarch64.
		armv8_aes = getauxval(AT_HWCAP) & HWCAP_AES;
		armv8_sha1 = getauxval(AT_HWCAP) & HWCAP_SHA1;
		armv8_sha2 = getauxval(AT_HWCAP) & HWCAP_SHA2;
		armv8_crc32 = getauxval(AT_HWCAP) & HWCAP_CRC32;
	#endif
#endif
	}

#if SOUP_X86
	void CpuInfo::invokeCpuid(void* out, uint32_t eax) noexcept
	{
	#if defined(_MSC_VER) && !defined(__clang__)
		__cpuid(((int*)out), eax);
		std::swap(((int*)out)[2], ((int*)out)[3]);
	#else
		__cpuid(eax, ((int*)out)[0], ((int*)out)[1], ((int*)out)[3], ((int*)out)[2]);
	#endif
	}

	void CpuInfo::invokeCpuid(void* out, uint32_t eax, uint32_t ecx) noexcept
	{
	#if defined(_MSC_VER) && !defined(__clang__)
		__cpuidex(((int*)out), eax, ecx);
		std::swap(((int*)out)[2], ((int*)out)[3]);
	#else
		__cpuid_count(eax, ecx, ((int*)out)[0], ((int*)out)[1], ((int*)out)[3], ((int*)out)[2]);
	#endif
	}
#endif
}

#endif
