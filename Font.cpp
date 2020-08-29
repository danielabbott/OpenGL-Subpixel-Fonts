#include "Font.h"
#include "Assert.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H

using namespace std;


namespace SubPixelFonts {

	static FT_Library library;

	void Font::init() {
		assert__(!FT_Init_FreeType(&library), "Error initialising FreeType");
		FT_Library_SetLcdFilter(library, FT_LCD_FILTER_DEFAULT);
	}

	void Font::deinit() {
		FT_Done_FreeType(library);
	}

	static map<string, weak_ptr<Font>> fonts;

	shared_ptr<Font> Font::load(string const& file_path, bool load) {
		shared_ptr<Font> font_pointer = nullptr;

		auto font = fonts.find(file_path);

		if (font != fonts.end()) {
			font_pointer = (*font).second.lock();
		}

		if (font_pointer == nullptr) {
			// Create font
			font_pointer = make_shared<Font>(file_path);
			fonts.insert({ file_path, font_pointer });
		}

		return font_pointer;
	}

	Font::Font(string const& file_path, bool load) {
		if (load) {
			face = nullptr;
			assert__(!FT_New_Face(library,
				file_path.c_str(),
				0,
				(FT_Face*)&face.value()), "Error loading font");
		}
	}

	Font::~Font() {
		if (face.has_value()) {
			FT_Done_Face(reinterpret_cast<FT_Face>(face.value()));
		}
	}

	shared_ptr<FontInstance> Font::load_font_instance(FontHeight height_in_pixels, bool load) {
		shared_ptr<FontInstance> font_instance_pointer = nullptr;

		auto font_instance = instances.find(height_in_pixels);

		if (font_instance != instances.end()) {
			font_instance_pointer = (*font_instance).second.lock();
		}

		if (font_instance_pointer == nullptr) {
			// Create font instance
			font_instance_pointer = make_shared<FontInstance>(load ? face.value() : nullptr, height_in_pixels, load);
			instances.insert({ height_in_pixels, font_instance_pointer });
		}

		return font_instance_pointer;
	}

	FontInstance::FontInstance(FontFace face_, FontHeight height_in_pixels, bool load) {
		if (!load) {
			data_freed = true;
			return;
		}

		auto face = reinterpret_cast<FT_Face>(face_);

		auto size_in_inches = height_in_pixels / 96.0;
		auto size_in_points = size_in_inches * 72.0;

		assert_(!FT_Set_Char_Size(
			face,
			0,       /* char_width in 1/64th of points (=char_height)  */
			static_cast<long>(size_in_points * 64),   /* char_height in 1/64th of points */
			96,     /* ppi */
			0
		));



		auto f = [face, this](CharCode start, CharCode end) {
			for (CharCode char_code = start; char_code <= end; char_code++) {
				auto glyph_index = FT_Get_Char_Index(face, char_code);
				if (glyph_index <= 0) {
					continue;
				}
				glyphs.emplace(make_pair(char_code, Glyph(face, char_code, glyph_index)));
			}
		};

		f(32, 126);
		f(160, 255);
	}

	void FontInstance::free_data() {
		data_freed = true;
		for (auto& [_, glyph] : glyphs) {
			glyph.free_data();
		}
	}

	Glyph::Glyph(FontFace face_, CharCode c, uint32_t glyph_index)
	{
		auto face = reinterpret_cast<FT_Face>(face_);

		assert_(!FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT));

		// FT_RENDER_MODE_NORMAL for 8-bit grey fonts
		// FT_RENDER_MODE_LCD for RGB fonts for horizontal displays
		assert_(!FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD));

		auto bitmap = &face->glyph->bitmap;

		// FT_PIXEL_MODE_GRAY for 8-bit grey fonts
		// FT_PIXEL_MODE_LCD for RGB fonts for horizontal displays
		assert_(bitmap->pixel_mode == FT_PIXEL_MODE_LCD);

		advance = face->glyph->advance.x / 64;
		bitmap_width = bitmap->width / 3;
		bitmap_height = bitmap->rows;
		left = face->glyph->bitmap_left;
		top = face->glyph->bitmap_top;

		int pitch = bitmap->pitch; // width in bytes

		assert_(static_cast<unsigned>(pitch) >= bitmap_width * 3);

		if (get_bitmap_size_bytes() > 0) {
			bitmap_data = HeapArray<unsigned char>(get_bitmap_size_bytes());

			unsigned char* dst = bitmap_data.value().get();
			const unsigned char* src = bitmap->buffer;
			for (unsigned int y = 0; y < bitmap_height; y++) {
				for (unsigned int x = 0; x < bitmap_width; x++) {
					dst[0] = src[x * 3 + 0];
					dst[1] = src[x * 3 + 1];
					dst[2] = src[x * 3 + 2];
					dst[3] = 255;
					dst += 4;
				}
				src += pitch;
			}

		}
	}

	void Glyph::free_data() {
		if (bitmap_data.has_value()) {
			bitmap_data = nullopt;
		}
	}

	Glyph::~Glyph() {
		free_data();
	}
}
