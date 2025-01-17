#pragma once

#include "fwd.hpp"

#include <string>
#include <vector>

#include "lyoStyle.hpp"

#define LYO_DEBUG_POS		false
#define LYO_DEBUG_STYLE		false

NAMESPACE_SOUP
{
	struct lyoElement
	{
		lyoContainer* parent;

		unsigned int flat_x;
		unsigned int flat_y;
		unsigned int flat_width;
		unsigned int flat_height;

		std::string tag_name{};

		lyoStyle style;

		using on_click_t = void(*)(lyoElement& target, lyoDocument&);

		on_click_t on_click = nullptr;
		void(*on_char)(char32_t, lyoElement&, lyoDocument&) = nullptr;

		// This would be "EventTarget", but for now it's fine here.
		void(*on_input)(lyoElement&, lyoDocument&) = nullptr;

		lyoElement(lyoContainer* parent)
			: parent(parent)
		{
		}

		virtual ~lyoElement() = default;

		[[nodiscard]] lyoDocument& getDocument() noexcept;
		void focus() noexcept;
		[[nodiscard]] on_click_t getClickHandler() const noexcept;

		[[nodiscard]] bool matchesSelector(const std::string& selector) const noexcept;
		virtual void querySelectorAll(std::vector<lyoElement*>& res, const std::string& selector);

		virtual void propagateStyle();

		virtual void populateFlatDocument(lyoFlatDocument& fdoc);
		virtual void updateFlatValues(unsigned int& x, unsigned int& y, unsigned int& wrap_y);
		void setFlatPos(unsigned int x, unsigned int y);
		void wrapLine(unsigned int& x, unsigned int& y, unsigned int& wrap_y);

		virtual void draw(RenderTarget& rt) const;
	};
}
