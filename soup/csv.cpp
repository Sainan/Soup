#include "csv.hpp"

#include "base.hpp"

NAMESPACE_SOUP
{
	std::vector<std::string> csv::parseLine(const std::string& line)
	{
		std::vector<std::string> res{};
		res.reserve(10);
		size_t i = 0;
		while (i < line.length())
		{
			SOUP_IF_UNLIKELY(line.at(i) == '"')
			{
				i += 1;
				auto sep = line.find('"', i);
				SOUP_ASSERT(sep != std::string::npos); // must have closing quote
				res.emplace_back(line.substr(i, sep - i));
				i = sep + 2;
			}
			else
			{
				auto sep = line.find(',', i);
				if (sep == std::string::npos)
				{
					res.emplace_back(line.substr(i));
					break;
				}
				res.emplace_back(line.substr(i, sep - i));
				i = sep + 1;
			}
		}
		return res;
	}
}
