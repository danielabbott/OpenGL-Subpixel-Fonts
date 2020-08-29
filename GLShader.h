#pragma once

namespace SubPixelFonts {

	const char* const FONT_VERTEX_SHADER_GL3 = R"(
#version 130

uniform vec2 window_size;

in vec2 in_position; // window coordinates, 0,0 = top left
in vec3 in_font_tex_coord; // texture coordinates, 0,0,0 = top left first image

out vec3 pass_font_tex_coord;

void main() {
	vec2 c = (in_position / window_size);
	c.y = 1.0 - c.y;
	c = c * 2.0 - 1.0;
	gl_Position = vec4(c, 0.0, 1.0);

	pass_font_tex_coord = in_font_tex_coord;
}

)";


	const char* const FONT_FRAGMENT_SHADER_GL3 = R"(
#version 130

uniform sampler2DArray tex;

in vec3 pass_font_tex_coord;

out vec3 out_colour;

void main() {
	out_colour = texture(tex, vec3(pass_font_tex_coord.xy / vec2(textureSize(tex, 0).xy), pass_font_tex_coord.z)).rgb;}

)";

}