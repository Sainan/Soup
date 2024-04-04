#include "Pattern.hpp"

#include "Module.hpp"
#include "CompiletimePatternWithOptBytesBase.hpp"
#include "Pointer.hpp"
#include "string.hpp"

namespace soup
{
	Pattern::Pattern(const CompiletimePatternWithOptBytesBase& sig)
		: bytes(sig.getVec())
	{
	}

	Pattern::Pattern(const std::string& str)
		: Pattern(str.data(), str.size())
	{
	}

	Pattern::Pattern(const char* str, size_t len)
	{
		auto to_hex = [&](char c) -> std::optional<std::uint8_t>
		{
			switch (string::upper_char(c))
			{
			case '0':
				return static_cast<std::uint8_t>(0);
			case '1':
				return static_cast<std::uint8_t>(1);
			case '2':
				return static_cast<std::uint8_t>(2);
			case '3':
				return static_cast<std::uint8_t>(3);
			case '4':
				return static_cast<std::uint8_t>(4);
			case '5':
				return static_cast<std::uint8_t>(5);
			case '6':
				return static_cast<std::uint8_t>(6);
			case '7':
				return static_cast<std::uint8_t>(7);
			case '8':
				return static_cast<std::uint8_t>(8);
			case '9':
				return static_cast<std::uint8_t>(9);
			case 'A':
				return static_cast<std::uint8_t>(10);
			case 'B':
				return static_cast<std::uint8_t>(11);
			case 'C':
				return static_cast<std::uint8_t>(12);
			case 'D':
				return static_cast<std::uint8_t>(13);
			case 'E':
				return static_cast<std::uint8_t>(14);
			case 'F':
				return static_cast<std::uint8_t>(15);
			default:
				return std::nullopt;
			}
		};

		for (size_t i = 0; i < len; ++i)
		{
			if (str[i] == ' ')
			{
				continue;
			}

			const bool last = (i == len - 1);
			if (str[i] != '?')
			{
				if (!last)
				{
					auto c1 = to_hex(str[i]);
					auto c2 = to_hex(str[i + 1]);

					if (c1 && c2)
					{
						bytes.emplace_back(static_cast<std::uint8_t>((*c1 * 0x10) + *c2));
					}
				}
			}
			else
			{
				bytes.emplace_back(std::nullopt);
			}
		}
	}

	std::string Pattern::toString() const SOUP_EXCAL
	{
		std::string str;
		for (const auto& b : bytes)
		{
			if (b.has_value())
			{
				str.append(string::lpad(string::hex(b.value()), 2, '0'));
			}
			else
			{
				str.push_back('?');
			}
			str.push_back(' ');
		}
		if (!str.empty())
		{
			str.pop_back();
		}
		return str;
	}
}
