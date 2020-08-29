#include "TextureAtlas.h"
#include <stb_rect_pack.h>
#include "Assert.h"
#include <sstream>
#include <fstream>
#include <cstring>

#ifdef STB_IMAGE_AVAILABLE
#include <stb_image.h>
#endif

#ifdef LIB_WEBP_AVAILABLE
#include <webp/decode.h>
#endif

using namespace std;

namespace SubPixelFonts {

	TextureAtlas::TextureAtlas(unsigned int w, unsigned int h, vector<shared_ptr<FontInstance>> const& fonts, bool clear_background)
		: width(w), height(h)
	{

		// Count glyphs

		size_t glyphsTotal = 0;
		{
			for (auto const& fi : fonts) {
				if (fi->data_freed) {
					throw runtime_error("Font data has been freed");
				}

				glyphsTotal += fi->glyphs.size();

				all_glyph_data.emplace_back(fi);
			}
		}



		// Create array glyph objects
		// Create an array that maps a stbrp_rect.id to an array glyph reference
		// Create a stbrp_rect object for every glyph, plus one for the white pixel

		vector<stbrp_rect> rects;
		vector<AtlasGlyph*> id_to_glyph;
		{
			rects.reserve(glyphsTotal + 1);
			id_to_glyph.reserve(glyphsTotal);

			stbrp_rect r;
			r.id = -1;
			r.w = 1;
			r.h = 1;
			rects.push_back(r);

			for (auto& font_instance_data : all_glyph_data) {
				for (const auto& [char_code, glyph] : font_instance_data.font->glyphs) {
					// Create atlas glyph object
					auto& atlas_glyph = (*font_instance_data.map.insert(pair<CharCode, AtlasGlyph>(char_code, AtlasGlyph(&glyph))).first).second;

					if (glyph.bitmap_width && glyph.bitmap_height) {

						// Store reference to it in rect.id->glyph array
						id_to_glyph.push_back(&atlas_glyph);


						stbrp_rect r;
						r.id = static_cast<int>(id_to_glyph.size() - 1);
						r.w = glyph.bitmap_width;
						r.h = glyph.bitmap_height;
						rects.push_back(r);
					}
				}
			}
		}


		bool all_packed = false;
		vector<stbrp_node> nodes(width);
		stbrp_context context;

		while (!all_packed) {
			// New layer

			images.push_back(HeapArray<unsigned char>(width * height * 4));
			layers++;
			auto const& this_layer_image = images[images.size() - 1];

			if (clear_background) {
				memset(this_layer_image.get(), 0, this_layer_image.size());
			}


			// Pack as many as possible

			stbrp_init_target(&context, width, height, nodes.data(), static_cast<int>(nodes.size()));
			all_packed = stbrp_pack_rects(&context, rects.data(), static_cast<int>(rects.size())) != 0;

			// Put all the unpacked rects into rects2 and create glyph objects for the packed ones

			vector<stbrp_rect> rects2;

			if (!all_packed) {
				rects2.reserve(rects.size());
			}

			for (auto const& rect : rects) {
				if (rect.was_packed) {
					if (rect.id == -1) {
						white_pixel_layer = static_cast<unsigned>(images.size()) - 1;
						white_pixel_x = rect.x;
						white_pixel_y = rect.y;

						unsigned char* dst = &this_layer_image.get()[(white_pixel_y * width + white_pixel_x) * 4];
						dst[0] = dst[1] = dst[2] = 255;
					}
					else {
						// Create glyph object

						AtlasGlyph& atlas_glyph = *id_to_glyph[rect.id];
						atlas_glyph.bitmap_layer = static_cast<unsigned>(images.size()) - 1;
						atlas_glyph.bitmap_x = rect.x;
						atlas_glyph.bitmap_y = rect.y;

						Glyph const& glyph = *atlas_glyph.glyph;

						unsigned char* dst = this_layer_image.get();
						unsigned char* src = glyph.bitmap_data.value().get();

						assert_(atlas_glyph.bitmap_y + glyph.bitmap_height <= height);
						assert_(atlas_glyph.bitmap_x + glyph.bitmap_width <= width);

						for (unsigned y = 0; y < glyph.bitmap_height; y++) {
							unsigned dst_idx = ((y + atlas_glyph.bitmap_y) * width + (atlas_glyph.bitmap_x)) * 4;

							memcpy(&dst[dst_idx], &src[y * glyph.bitmap_width * 4], glyph.bitmap_width * 4);
						}
					}
				}
				else {
					rects2.push_back(rect);
				}
			}

			rects = move(rects2);
		}


	}

	string TextureAtlas::get_glyph_data(unsigned int font_index) {
		const auto& font_data = all_glyph_data[font_index];

		ostringstream s;
		s << "charcode,bitmap_x,bitmap_y,bitmap_layer,advance,bitmap_width,bitmap_height,left,top\n";
		s << "0," << to_string(white_pixel_x) << ',' << to_string(white_pixel_y) << ',' << to_string(white_pixel_layer)
			<< ",0,1,1,0,0\n";


		for (const auto& [char_code, atlas_glyph] : font_data.map) {
			const auto& glyph = *atlas_glyph.glyph;

			s << to_string(char_code)
				<< ',' << to_string(atlas_glyph.bitmap_x)
				<< ',' << to_string(atlas_glyph.bitmap_y)
				<< ',' << to_string(atlas_glyph.bitmap_layer)
				<< ',' << to_string(glyph.advance)
				<< ',' << to_string(glyph.bitmap_width)
				<< ',' << to_string(glyph.bitmap_height)
				<< ',' << to_string(glyph.left)
				<< ',' << to_string(glyph.top)
				<< '\n';
		}

		return s.str();

	}

	void TextureAtlas::save_all_glyph_data(string const& csv_file_path_without_suffix)
	{
		for (unsigned int i = 0; i < all_glyph_data.size(); i++) {
			ofstream f((csv_file_path_without_suffix + to_string(i) + string(".csv")).c_str(), ios::out);
			f << get_glyph_data(i);
		}
	}

	TextureAtlas::TextureAtlas(unsigned int w, unsigned int h, ImageVector&& images_, vector<CachedFontData>&& fonts)
		: width(w), height(h), images(move(images_)) {
		do_init(move(fonts));
	}

	void TextureAtlas::do_init(vector<CachedFontData>&& fonts)
	{
		for (auto const& font : fonts) {
			auto font_ptr = Font::load(font.path, false);
			auto font_instance_ptr = font_ptr->load_font_instance(font.height, false);

			all_glyph_data.emplace_back(font_instance_ptr);

			auto& font_data = all_glyph_data[all_glyph_data.size() - 1];




			istringstream s(font.glyph_data);

			const char* const EXCEPTION_INVALID_CSV = "Invalid texture atlas cache CSV file";

			const char* const first_line = "charcode,bitmap_x,bitmap_y,bitmap_layer,advance,bitmap_width,bitmap_height,left,top";

			string line;
			if (!getline(s, line) || line.rfind(first_line, 0) != 0) {
				throw runtime_error(EXCEPTION_INVALID_CSV);
			}

			if (line.size() != strlen(first_line) && line.size() != strlen(first_line) + 1 && line[line.size() - 1] != '\r') {
				throw runtime_error(EXCEPTION_INVALID_CSV);
			}


			while (getline(s, line)) {
				istringstream ss(line);

				string value;

				auto next = [&ss, &value, EXCEPTION_INVALID_CSV]() {
					if (!getline(ss, value, ',')) {
						throw runtime_error(EXCEPTION_INVALID_CSV);
					}
				};

				next();
				CharCode char_code = stoi(value);

				if (char_code == 0) {
					next();
					white_pixel_x = stoi(value);

					next();
					white_pixel_y = stoi(value);

					next();
					white_pixel_layer = stoi(value);
					next();
					next();
					next();
					next();
					next();
				}
				else {
					auto& glyph = (*font_instance_ptr->glyphs.insert(pair<CharCode, Glyph>(char_code, Glyph())).first).second;

					auto& atlas_glyph = (*font_data.map.insert(pair<CharCode, AtlasGlyph>(char_code, AtlasGlyph(&glyph))).first).second;
					atlas_glyph.glyph = &glyph;

					next();
					atlas_glyph.bitmap_x = stoi(value);

					next();
					atlas_glyph.bitmap_y = stoi(value);

					next();
					atlas_glyph.bitmap_layer = stoi(value);

					if (atlas_glyph.bitmap_layer >= images.size()) {
						throw runtime_error("Glyph bitmap layer out of range");
					}

					next();
					glyph.advance = stoi(value);

					next();
					glyph.bitmap_width = stoi(value);

					next();
					glyph.bitmap_height = stoi(value);

					next();
					glyph.left = stoi(value);

					next();
					glyph.top = stoi(value);
				}
			}
		}


	}

	static HeapArray<unsigned char> load_file(ifstream& in) {
		in.seekg(0, std::ios::end);
		HeapArray<unsigned char> d(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(reinterpret_cast<char*>(d.get()), d.size());
		in.close();
		return d;
	}

	// Load the file in binary or it will have 0s at the end
	static string load_file_str(ifstream& in) {
		in.seekg(0, std::ios::end);
		string s;
		s.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&s[0], s.size());
		in.close();
		return s;
	}

#if defined(LIB_WEBP_AVAILABLE) || defined(STB_IMAGE_AVAILABLE)
	TextureAtlas::TextureAtlas(unsigned int w, unsigned int h, string const& image_file_path_no_suffix,
		string const& csv_file_path_no_suffix, vector<pair<string, FontHeight>>&& fonts)
		: width(w), height(h)
	{

		while (1) {
#ifdef LIB_WEBP_AVAILABLE
			ifstream f_webp(image_file_path_no_suffix + to_string(images.size()) + string(".webp"), ios::in | ios::binary);

			if (f_webp.good()) {
				// Webp file exists, use that.

				auto data = load_file(f_webp);
				int actual_width, actual_height;
				bool success = WebPGetInfo(data.get(), data.size(), &actual_width, &actual_height) != 0;

				if (!success) {
					throw runtime_error("Invalid Webp file");
				}

				if (actual_width != width || actual_height != height) {
					throw runtime_error("Webp is wrong size");
				}

				uint8_t* decoded_data = WebPDecodeRGBA(data.get(), data.size(), &actual_width, &actual_height);

				if (!decoded_data) {
					throw runtime_error("Invalid Webp file");
				}

				images.push_back(HeapArray<unsigned char>(decoded_data, width * height * 4, [](unsigned char* p) {
					WebPFree(p);
				}));
				layers++;

				continue;
			}
#endif
#ifdef STB_IMAGE_AVAILABLE
			ifstream f_png(image_file_path_no_suffix + to_string(images.size()) + string(".png"), ios::in | ios::binary);
			if (f_png.good()) {
				auto data = load_file(f_png);
				int actual_width, actual_height;
				int channels_in_file;
				uint8_t* decoded_data = stbi_load_from_memory(data.get(), static_cast<int>(data.size()), &actual_width, &actual_height, &channels_in_file, 4);

				if (!decoded_data) {
					throw runtime_error("Invalid PNG file");
				}

				if (actual_width != width || actual_height != height) {
					throw runtime_error("PNG is wrong size");
				}

				images.push_back(HeapArray<unsigned char>(decoded_data, width * height * 4, [](unsigned char* p) {
					stbi_image_free(p);
				}));
				layers++;

				continue;
			}
#endif

			break;
		}

		if (images.size() == 0) {
			throw runtime_error("No images found");
		}

		vector<CachedFontData> font_data;

		unsigned int i = 0;
		for (auto& f : fonts) {
			CachedFontData fd;
			fd.path = move(f.first);
			fd.height = f.second;

			ifstream fs(csv_file_path_no_suffix + to_string(i++) + string(".csv"), ios::in | ios::binary);
			if (!fs.good()) {
				throw runtime_error("Missing .csv file");
			}

			fd.glyph_data = load_file_str(fs);

			font_data.push_back(move(fd));

		}



		do_init(move(font_data));
	}
#endif
}
