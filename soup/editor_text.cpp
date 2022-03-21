#include "editor_text.hpp"

#include "console.hpp"
#include "unicode.hpp"

namespace soup
{
	void editor_text::draw() const
	{
		console.setCursorPos(x, y);
		console.setBackgroundColour(0, 0, 0);
		console.setForegroundColour(255, 255, 255);
		for (const auto& line : file.contents)
		{
			console << unicode::utf32_to_utf8(line) << "\n";
		}
	}
}
