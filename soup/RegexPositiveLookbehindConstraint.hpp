#pragma once

#include "RegexConstraintTransitionable.hpp"

#include "RegexMatcher.hpp"

namespace soup
{
	struct RegexPositiveLookbehindConstraint : public RegexConstraintTransitionable
	{
		RegexGroup group;
		size_t window;

		RegexPositiveLookbehindConstraint(const RegexGroup::ConstructorState& s)
			: group(s, true)
		{
		}

		[[nodiscard]] const RegexConstraintTransitionable* getTransition() const noexcept final
		{
			return group.initial;
		}

		[[nodiscard]] bool matches(RegexMatcher& m) const noexcept final
		{
			if (std::distance(m.begin, m.it) < window)
			{
				return false;
			}
			m.it -= window;
			return true;
		}

		[[nodiscard]] std::string toString() const noexcept final
		{
			auto str = group.toString();
			str.insert(0, "(?<=");
			str.push_back(')');
			return str;
		}

		void getFlags(uint16_t& set, uint16_t& unset) const noexcept final
		{
			group.getFlags(set, unset);
		}
	};
}
