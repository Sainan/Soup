#pragma once

#include "RegexConstraint.hpp"

#include <vector>

#include "UniquePtr.hpp"

namespace soup
{
	struct RegexRepConstraint : public RegexConstraint
	{
		std::vector<UniquePtr<RegexConstraint>> constraints;

		[[nodiscard]] const RegexConstraintTransitionable* getTransition() const noexcept final
		{
			return constraints.at(0)->getTransition();
		}

		[[nodiscard]] std::string toString() const noexcept final
		{
			std::string str = constraints.at(0)->toString();
			str.push_back('{');
			str.append(std::to_string(constraints.size()));
			str.push_back('}');
			return str;
		}

		[[nodiscard]] size_t getCursorAdvancement() const final
		{
			return constraints.at(0)->getCursorAdvancement() * constraints.size();
		}
	};
}
