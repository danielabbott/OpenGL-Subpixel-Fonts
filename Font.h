#pragma once

#include <memory>
#include <string>
#include <map>
#include <optional>
#include <vector>
#include <cstdint>
#include "HeapArray.h"

namespace SubPixelFonts {

	// Font objects represents the .ttf (or whatever it be) file in memory
	// Font instances store all glyph data for a font with a given size
	// Font instances can continue to exist after the font object has been freed. They are fully independent.
	// Font objects cache weak pointers to font instances.


	using CharCode = uint32_t;
	using FontFace = void*; // FT_Face
	using FontHeight = unsigned int; // pixels

	struct Glyph {

		// How far to move cursor after drawing glyph
		unsigned int advance = 0;

		std::optional<HeapArray<unsigned char>> bitmap_data = std::nullopt;

		// If either of these is 0 then the glyph is invisible (space character etc.)
		unsigned int bitmap_width = 0;
		unsigned int bitmap_height = 0;

		// Position of glyph relative to baseline
		int left = 0;
		int top = 0;

		Glyph() {} // Blank glyph
		Glyph(FontFace, CharCode, uint32_t glyph_index);
		~Glyph();

		unsigned int get_bitmap_size_bytes() {
			// rgba, alpha always 1
			return bitmap_width * bitmap_height * 4;
		}

		void free_data();

		/*Glyph(const Glyph& other) {
			memcpy(this, &other, sizeof(Glyph));
			if (bitmap_data) {
				bitmap_data = new unsigned char[get_bitmap_size_bytes()];
				memcpy(bitmap_data.value(), other.bitmap_data.value(), get_bitmap_size_bytes());
			}
		}*/
		Glyph(const Glyph&) = delete;
		Glyph& operator=(const Glyph&) = delete;

		Glyph(Glyph&& other) = default;
		Glyph& operator=(Glyph&& other) = default;
	};

	class TextureAtlas;
	class FontInstance {
		friend class TextureAtlas;
	public:
		FontInstance(FontFace, FontHeight height_in_pixels, bool load = true); // Do not call this. Use Font.load_font_instance(..)

		void free_data();
	private:
		// If so then texture atlasses cannot be created using this font
		bool data_freed = false;

		std::map<CharCode, Glyph> glyphs; // ASCII code -> glyph
	};


	class Font {
		friend class FontInstance;
		friend struct Glyph;
	public:
		// !! Must be called before creating any fonts !!
		static void init();

		// Call before application quits
		static void deinit();

		// If load is false then the font object will be created but the .ttf file won't actually be loaded
		// ^ That is used when loading cached texture atlasses
		static std::shared_ptr<Font> load(std::string const& file_path, bool load = true);

		Font(std::string const& file_path, bool load = true); // Do not call this. Use the static factory function load(..)

		// If load is false then no glyphs will be loaded (used for loading from texture atlas cache)
		std::shared_ptr<FontInstance> load_font_instance(FontHeight height_in_pixels, bool load = true);

		~Font();

		Font(const Font&) = delete;
		Font& operator=(const Font&) = delete;

		Font(Font&&) = delete;
		Font& operator=(Font&&) = delete;
	private:

		std::optional<FontFace> face = std::nullopt; // Initialised in constructor

		std::map<FontHeight, std::weak_ptr<FontInstance>> instances;
	};

}
