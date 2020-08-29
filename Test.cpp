#include "Font.h"
#include "SaferRawPointer.h"
#include "TextureAtlas.h"
#include <vector>
#include <iostream>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "GLShader.h"

#pragma warning( disable : 4838 4244 )

using namespace std;
using namespace SubPixelFonts;


const auto fonts_to_load = vector<pair<string, FontHeight>>{
{
	"Lato-Regular.ttf", 32
},
{
"Lato-Regular.ttf", 16
},
{
"Lato-Bold.ttf", 16
}
};

auto load_fonts() {
	// Fonts loaded here must match fonts_to_load
	auto lato = Font::load("Lato-Regular.ttf");
	auto lato_bold = Font::load("Lato-Bold.ttf");

	return vector<shared_ptr<FontInstance>> {
		lato->load_font_instance(32), lato->load_font_instance(16), lato_bold->load_font_instance(16)
	};
	// lato and lato_bold get deleted. that is fine.
}

void show_opengl_window(TextureAtlas& atlas);

void example() {
	Font::init();

	TextureAtlas* atlas = nullptr;

	try {
#if defined(LIB_WEBP_AVAILABLE) || defined(STB_IMAGE_AVAILABLE)
		// Load atlas if it is cached
		auto load = fonts_to_load;
		atlas = new TextureAtlas(256, 256, "atlas", "atlas", move(load));
#else
		throw runtime_error("");
#endif
	}
	catch (runtime_error e) {
		cerr << "Cached texture atlas either failed to load or does not exist (yet). Error: " << e.what() << '\n';

		// Generate and save atlas

		auto my_fonts = load_fonts();

		atlas = new TextureAtlas(256, 256, my_fonts);

		for (auto& f : my_fonts) {
			f->free_data(); // Free copy of memory used for creating atlases
		}

#if defined(LIB_WEBP_AVAILABLE) || defined(STB_IMAGE_WRITE_AVAILABLE)
#ifdef LIB_WEBP_AVAILABLE
		//atlas.save_webp("atlas");
#else
		atlas->save_png("atlas");
#endif
		atlas->save_all_glyph_data("atlas");
#endif
	}

	show_opengl_window(*atlas);

	Font::deinit();
}


void show_opengl_window(TextureAtlas& atlas) {
	// Create window


	assert_(glfwInit());

	glfwSetErrorCallback([](int e, const char* s) {
		cerr << "GLFW ERROR (" << e << "): " << s << '\n';
	});

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // gl3.2+
	glfwWindowHint(GLFW_STENCIL_BITS, 0);
	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_SRGB_CAPABLE, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
	glfwWindowHint(GLFW_SAMPLES, 0);
	auto window = glfwCreateWindow(800, 600, "Subpixel Fonts", nullptr, nullptr);
	assert_(window);

	glfwMakeContextCurrent(window);
	gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
	glfwSwapInterval(2);

	glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, 800, 600);
	glDisable(GL_MULTISAMPLE);
	glEnable(GL_FRAMEBUFFER_SRGB);

	glEnable(GL_BLEND);
	glBlendFunc(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR);	// sfactor, dfactor

	glClearColor(0, 0, 0, 1);
	assert_(glGetError() == GL_NO_ERROR);



	// Array Texture

	GLuint array_tex_id;
	glGenTextures(1, &array_tex_id);
	assert_(array_tex_id);

	glBindTexture(GL_TEXTURE_2D_ARRAY, array_tex_id);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, atlas.get_width(), atlas.get_height(), static_cast<GLsizei>(atlas.get_image_data().size()), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	{ // Upload data
		unsigned int i = 0;
		for (const auto& img : atlas.get_image_data()) {
			assert_(img.size() == atlas.get_width() * atlas.get_height() * 4);
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, atlas.get_width(), atlas.get_height(), 1, GL_RGBA, GL_UNSIGNED_BYTE, img.get());

			i++;
		}
		atlas.free_data();
	}
	assert_(glGetError() == GL_NO_ERROR);



	// Shader program


	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	const char* src = FONT_VERTEX_SHADER_GL3;
	glShaderSource(vs, 1, &src, nullptr);
	glCompileShader(vs);
	GLint status;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
	assert_(status);


	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	src = FONT_FRAGMENT_SHADER_GL3;
	glShaderSource(fs, 1, &src, nullptr);
	glCompileShader(fs);
	glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
	assert_(status);

	auto shader_program = glCreateProgram();
	glAttachShader(shader_program, vs);
	glAttachShader(shader_program, fs);
	glBindAttribLocation(shader_program, 0, "in_position");
	glBindAttribLocation(shader_program, 1, "in_font_tex_coord");
	glLinkProgram(shader_program);
	glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
	assert_(status);

	glDeleteShader(vs);
	glDeleteShader(fs);

	glUseProgram(shader_program);
	glUniform1i(glGetUniformLocation(shader_program, "tex"), 0);
	glUniform2f(glGetUniformLocation(shader_program, "window_size"), 800, 600);
	assert_(status);


	// Vertices

	const char* const lorum = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui o]ff[icia deserunt mollit anim id est laborum";

	const string text1[3] = { "The quick brown fox jumped over the lazy dog." , lorum, lorum };

	struct Vertex {
		float position[2];
		float font_tex_coord[3];
	};

	vector<Vertex> vertices_white_bg;
	vector<Vertex> vertices_purple_bg;
	vector<Vertex> vertices_white_font;
	vector<Vertex> vertices_black_font;
	vector<Vertex> vertices_red_font;

	vector<Vertex>* all_v_arrs[5] = { &vertices_white_bg, &vertices_purple_bg, &vertices_white_font,
		&vertices_black_font, &vertices_red_font };

	// One for each vector of vertices
	float colours[5][3] = { {1,1,1}, {0.78f,0.75f,0.91f}, {1,1,1}, {0,0,0}, {0.2f,0,0} };

	for(unsigned int i = 1; i < 3; i++) { // background colours
		float y = i*200.0f;
		Vertex tr = {
			{800, y},
			{atlas.white_px_x(), atlas.white_px_y(), atlas.white_px_layer()},
		};

		Vertex tl = {
			{0, y},
			{atlas.white_px_x(), atlas.white_px_y(), atlas.white_px_layer()},
		};

		Vertex bl = {
			{0, y+200},
			{atlas.white_px_x(), atlas.white_px_y(), atlas.white_px_layer()},
		};

		Vertex br = {
			{800, y+200},
			{atlas.white_px_x(), atlas.white_px_y(), atlas.white_px_layer()},
		};

		auto v = i == 1 ? &vertices_white_bg : &vertices_purple_bg;
		auto & vertices = *v;


		vertices.push_back(tl);
		vertices.push_back(tr);
		vertices.push_back(br);
		vertices.push_back(br);
		vertices.push_back(bl);
		vertices.push_back(tl);
	}

	vector<Vertex>* vector_to_use[3] = { &vertices_white_font, &vertices_black_font, &vertices_red_font };

	for (unsigned int i = 0; i < 3; i++) {
		int x = 10;
		int y = i*200 + 80;

		for (char c : text1[i]) {
			auto const& glyph_map = atlas.get_glyph_map(i);
			auto iter = glyph_map.find(static_cast<CharCode>(c));

			if (iter == glyph_map.end()) {
				// Whitespace character?

				auto const& glyph_map2 = atlas.get_font_glyph_map(i);
				auto iter2 = glyph_map2.find(static_cast<CharCode>(c));

				if (iter2 != glyph_map2.end()) {
					x += (*iter2).second.advance;
				}

				continue;
			}

			auto const& atlas_glyph = (*iter).second;
			auto const& glyph = *atlas_glyph.glyph;

			x += glyph.left;
			y -= glyph.top;

			Vertex tr = {
				{x + glyph.bitmap_width, y},
				{atlas_glyph.bitmap_x + glyph.bitmap_width, atlas_glyph.bitmap_y, atlas_glyph.bitmap_layer},
			};

			Vertex tl = {
				{x, y},
				{atlas_glyph.bitmap_x, atlas_glyph.bitmap_y, atlas_glyph.bitmap_layer},
			};

			Vertex bl = {
				{x, y + glyph.bitmap_height},
				{atlas_glyph.bitmap_x, atlas_glyph.bitmap_y + glyph.bitmap_height, atlas_glyph.bitmap_layer},
			};

			Vertex br = {
				{x + glyph.bitmap_width, y + glyph.bitmap_height},
				{atlas_glyph.bitmap_x + glyph.bitmap_width, atlas_glyph.bitmap_y + glyph.bitmap_height, atlas_glyph.bitmap_layer},
			};

			auto& vertices = *vector_to_use[i];

			vertices.push_back(tl);
			vertices.push_back(tr);
			vertices.push_back(br);
			vertices.push_back(br);
			vertices.push_back(bl);
			vertices.push_back(tl);

			x -= glyph.left;
			y += glyph.top;

			x += glyph.advance;

			if (x > 780) {
				x = 10;
				y += 24;
			}
		}
		y += 100;
	}

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(Vertex), nullptr);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(5 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(8 * sizeof(float)));
	glEnableVertexAttribArray(3);
	assert_(status);





	while (!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);

		unsigned int i = 0;
		for (auto v : all_v_arrs) {
			glBufferData(GL_ARRAY_BUFFER, v->size() * sizeof(Vertex), v->data(), GL_DYNAMIC_DRAW);
			assert_(status);
			glBlendColor(colours[i][0], colours[i][1], colours[i][2], 1);
			glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(v->size()));

			i++;
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
}

int main() {
	example();
	return 0;
}