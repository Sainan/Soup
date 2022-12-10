#include "string.hpp"

#include <filesystem>
#include <fstream>
#include <streambuf>

namespace soup
{
	std::string string::join(const std::vector<std::string>& arr, const char glue)
	{
		std::string res{};
		if (!arr.empty())
		{
			res = arr.at(0);
			for (size_t i = 1; i != arr.size(); ++i)
			{
				res.push_back(glue);
				res.append(arr.at(i));
			}
		}
		return res;
	}
	
	std::string string::join(const std::vector<std::string>& arr, const std::string& glue)
	{
		std::string res{};
		if (!arr.empty())
		{
			res = arr.at(0);
			for (size_t i = 1; i != arr.size(); ++i)
			{
				res.append(glue);
				res.append(arr.at(i));
			}
		}
		return res;
	}

	void string::listAppend(std::string& str, std::string&& add)
	{
		if (str.empty())
		{
			str = std::move(add);
		}
		else
		{
			str.append(", ").append(add);
		}
	}

	std::string string::_xor(const std::string& l, const std::string& r)
	{
		if (l.size() < r.size())
		{
			return _xor(r, l);
		}
		// l.size() >= r.size()
		std::string res(l.size(), '\0');
		for (size_t i = 0; i != l.size(); ++i)
		{
			res.at(i) = (char)((uint8_t)l.at(i) ^ (uint8_t)r.at(i % r.size()));
		}
		return res;
	}

	std::string string::xorSameLength(const std::string& l, const std::string& r)
	{
		std::string res(l.size(), '\0');
		for (size_t i = 0; i != l.size(); ++i)
		{
			res.at(i) = (char)((uint8_t)l.at(i) ^ (uint8_t)r.at(i));
		}
		return res;
	}

	std::string string::fromFile(const std::string& file)
	{
		std::string ret{};
		if (std::filesystem::exists(file))
		{
			std::ifstream t(file, std::ios::binary);

			t.seekg(0, std::ios::end);
			ret.reserve(t.tellg());
			t.seekg(0, std::ios::beg);

			ret.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
		}
		return ret;
	}
}
