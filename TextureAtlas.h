#pragma once

// (Un)Comment these as appropriate
#define STB_IMAGE_WRITE_AVAILABLE
#define STB_IMAGE_AVAILABLE
//#define LIB_WEBP_AVAILABLE

#include "Font.h"
#include <vector>
#include <map>
#include <stdexcept>
#include "HeapArray.h"

#ifdef STB_IMAGE_WRITE_AVAILABLE
#include <stb_image_write.h>
#endif

#ifdef LIB_WEBP_AVAILABLE
#include <webp/encode.h>
#include <fstream>
#endif


namespace SubPixelFonts {

	struct AtlasGlyph {
		// If either glyph->bitmap_width or glyph->bitmap_height is 0 then the glyph is whitespace and
		// the x/y/layer variables are meaningless.

		unsigned int bitmap_x = 0;
		unsigned int bitmap_y = 0;
		unsigned bitmap_layer = 0;

		// Pointer is set in TextureAtlas constructor. Pointer has same lifetime as font object (TextureAtlas::FontInstanceData::font)
		const Glyph* glyph;

		AtlasGlyph(const Glyph* glyph_) : glyph(glyph_) {}
	};

	class TextureAtlas {
	public:
		using ImageVector = std::vector<HeapArray<unsigned char>>;


		// If clear_background is false then all unused space in the texture is uninitialised and might not compress well if exported to a .png
		TextureAtlas(unsigned int width, unsigned int height, std::vector<std::shared_ptr<FontInstance>> const&, bool clear_background = true);

		struct CachedFontData {
			std::string path;
			FontHeight height;
			std::string glyph_data; // Contents of .csv file
		};

		// Fonts don't get loaded, but if they haven't been loaded in yet then they will be initialised from the glyph csv file
		// Load cachced texture atlases after loading any fonts in the regular way
		TextureAtlas(unsigned int width, unsigned int height, ImageVector&&, std::vector<CachedFontData>&& fonts);

#if defined(LIB_WEBP_AVAILABLE) || defined(STB_IMAGE_AVAILABLE)
		TextureAtlas(unsigned int width, unsigned int height, std::string const& image_file_path_no_suffix,
			std::string const& csv_file_path_no_suffix, std::vector<std::pair<std::string, FontHeight>>&&);
#endif


		//TextureAtlas(unsigned int width, unsigned int height, ImageVector&&, std::vector<std::shared_ptr<FontInstance>> const&);

		AtlasGlyph const& get_glyph_position(std::shared_ptr<FontInstance> const&, FontHeight, CharCode);

		// Returns string containing text representation of all glyphs (including position in the bitmap)
		std::string get_glyph_data(unsigned int font_index);

		// If csv_file_path_without_suffix is "/a/b/c/file" then the files generated for a 2-font image would be:
		// "/a/b/c/file1.csv"
		// "/a/b/c/file2.csv"
		// Indexes correspond to indexes into the vector that was passed to the constructor
		void save_all_glyph_data(std::string const& csv_file_path_without_suffix);

#ifdef STB_IMAGE_WRITE_AVAILABLE
		// If file_path_without_suffix is "/a/b/c/file" then the files generated for a 2-layer image would be:
		// "/a/b/c/file1.png"
		// "/a/b/c/file2.png"
		void save_png(std::string const& file_path_without_suffix) {
			unsigned int i = 0;
			for (const auto& image : images) {
				if (!stbi_write_png((file_path_without_suffix + std::to_string(i) + std::string(".png")).c_str(), width, height, 4, image.get(), width * 4)) {
					throw std::runtime_error("stb_image_write error");
				}

				i++;
			}
		}
#endif

#ifdef LIB_WEBP_AVAILABLE
		void save_webp(std::string const& file_path_without_suffix) {
			unsigned int i = 0;
			for (const auto& image : images) {
				uint8_t* out;
				auto size = WebPEncodeLosslessRGBA(image.get(), width, height, width * 4, &out);
				if (!size) {
					throw std::runtime_error("libwebp error");
				}

				std::ofstream f(file_path_without_suffix + std::to_string(i) + std::string(".webp"), std::ios::out | std::ios::binary);
				f.write(reinterpret_cast<const char*>(out), size);

				WebPFree(out);

				i++;
			}
		}
#endif

		ImageVector const& get_image_data() const {
			return images;
		}

		unsigned int get_width() const { return width; }
		unsigned int get_height() const { return height; }
		unsigned int get_layers() const { return layers; }

		void free_data() {
			images = ImageVector();
		}

		std::map<CharCode, AtlasGlyph> const& get_glyph_map(unsigned int font_index) {
			return all_glyph_data[font_index].map;
		}

		std::map<CharCode, Glyph> const& get_font_glyph_map(unsigned int font_index) {
			return all_glyph_data[font_index].font->glyphs;
		}



		std::map<CharCode, AtlasGlyph> const& get_glyph_map(FontInstance const& font) {
			for (const auto& f : all_glyph_data) {
				if (f.font.get() == &font) {
					return f.map;
				}
			}
			throw std::runtime_error("Font instance not found");
		}

		std::map<CharCode, Glyph> const& get_font_glyph_map(FontInstance const& font) {
			for (const auto& f : all_glyph_data) {
				if (f.font.get() == &font) {
					return f.font->glyphs;
				}
			}
			throw std::runtime_error("Font instance not found");
		}

		unsigned int white_px_x() {
			return white_pixel_x;
		}

		unsigned int white_px_y() {
			return white_pixel_y;
		}

		unsigned int white_px_layer() {
			return white_pixel_layer;
		}

	private:
		unsigned int width, height, layers;

		struct FontInstanceData {
			std::shared_ptr<FontInstance> font; // AtlasGlyphs rely on this shared pointer keeping the FontInstance alive
			std::map<CharCode, AtlasGlyph> map;

			FontInstanceData(std::shared_ptr<FontInstance> const& f) : font(f) {}
			FontInstanceData(std::shared_ptr<FontInstance>&& f) : font(move(f)) {}
		};

		std::vector<FontInstanceData> all_glyph_data;

		// If this is empty then free_data() has been called. Use get_layers() instead of images.size().
		ImageVector images;

		unsigned white_pixel_x = 0, white_pixel_y = 0, white_pixel_layer = 0;

		void do_init(std::vector<CachedFontData>&& fonts);
	};
}
