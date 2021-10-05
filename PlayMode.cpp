#include "PlayMode.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include "data_path.hpp"

// dialogue tree asset

#include <map>

std::map<uint64_t, Node> node_map;
std::vector<std::string> stage;

PlayMode::Line::Line(ColorTextureProgram& color_texture_program, const std::string& font_path, int y) {
	ypos = y;

	{
		glGenBuffers(1, &vertex_buffer);
		GL_ERRORS();
	}

	{
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);
		GL_ERRORS();
		glBindVertexArray(vertex_buffer_for_color_texture_program);
		GL_ERRORS();
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		GL_ERRORS();
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 0 //offset
		);
		GL_ERRORS();
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		GL_ERRORS();
		// glVertexAttribPointer(
		// 	color_texture_program.Color_vec4, //attribute
		// 	4, //size
		// 	GL_UNSIGNED_BYTE, //type
		// 	GL_TRUE, //normalized
		// 	sizeof(Vertex), //stride
		// 	(GLbyte *)0 + 4*3 //offset
		// );
		// GL_ERRORS();
		// glEnableVertexAttribArray(color_texture_program.Color_vec4);
		// GL_ERRORS();
		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 + 4*1 //offset
		);
		GL_ERRORS();
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);
		GL_ERRORS();
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		GL_ERRORS();
		glBindVertexArray(0);
		GL_ERRORS();
	}

	{
		auto error = FT_Init_FreeType(&ft_library);
		if (error) {
			std::cerr << "FreeType errored out\n";
		}
		else {
			std::cerr << "FreeType running\n";
		}

		error = FT_New_Face(ft_library, font_path.c_str(), 0, &ft_face);
		if (error == FT_Err_Cannot_Open_Resource) {
			std::cerr << "Cannot open resource\n";
		}
		else if (error){
			std::cerr << "Error loading face.\n" << error;
		}

		error = FT_Set_Char_Size(ft_face, 0, 16*64, 300, 300);
		if (error) {
			std::cerr << "Error setting charsize.\n";
		}

		hb_font = hb_ft_font_create(ft_face, NULL);
		hb_buffer = hb_buffer_create();
	}
}

void PlayMode::Line::setText(const std::string& text) {
	// reset harfbuzz
	hb_buffer_reset(hb_buffer);

	hb_buffer_add_utf8(hb_buffer, text.c_str(), -1, 0, -1);
	hb_buffer_guess_segment_properties(hb_buffer);

	hb_shape(hb_font, hb_buffer, NULL, 0);
	unsigned buffer_length = hb_buffer_get_length(hb_buffer);

	hb_glyph_info_t* info = hb_buffer_get_glyph_infos(hb_buffer, NULL);
	hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(hb_buffer, NULL);
	
	glm::uvec2 text_size = glm::uvec2(2500, 80);
	std::vector<uint8_t> text_texture(text_size.x * text_size.y, 0x00);	
	unsigned current_x = 0;

	for (unsigned i = 0; i < buffer_length; i++) {
		hb_codepoint_t gid = info[i].codepoint;
		auto cluster = info[i].cluster;
		double x_advance = pos[i].x_advance/64.;
		double y_advance = pos[i].y_advance/64.;
		double x_offset = pos[i].x_offset/64.;
		double y_offset = pos[i].y_offset/64.;

		FT_Load_Glyph(ft_face, gid, FT_LOAD_RENDER);
		const FT_Bitmap& bitmap = ft_face->glyph->bitmap;
		unsigned row_offset = 60 - ft_face->glyph->bitmap_top;
		unsigned col_offset = ft_face->glyph->bitmap_left;
		unsigned offset_x = unsigned(x_offset);
		unsigned offset_y = unsigned(y_offset);
		for (unsigned row = 0; row < bitmap.rows; row++) {
			for (unsigned col = 0; col < bitmap.width; col++) {
				// hopefully LICM will fix this
				text_texture[(row + row_offset + offset_y) * text_size.x + current_x + offset_x + col + col_offset] = bitmap.buffer[row * bitmap.width + col];
			}
		}

		current_x += unsigned(x_advance);
	}

	// create CRT effect
	for (unsigned row = 0; row < text_size.y; row++) {
		if (row % 6 == 0 || (row > 0 && (row-1) % 6 == 0) || (row > 1 && (row-2) % 6 == 0)) {
			for (unsigned col = 0; col < text_size.x; col++) {
				text_texture[row * text_size.x + col] = 0;
			}
		}
	}

	if (is_first_time) {
		glGenTextures(1, &current_tex);
		GL_ERRORS();
	}

	glBindTexture(GL_TEXTURE_2D, current_tex);
	GL_ERRORS();

	if (is_first_time) {
		// needed because our uint8_t texture isn't RGBA
		// not sure where I saw this, possibly https://learnopengl.com/In-Practice/Text-Rendering
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		GL_ERRORS();

		// adapted Pong code, seems to work
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, text_size.x, text_size.y, 0, GL_RED, GL_UNSIGNED_BYTE, text_texture.data());
		GL_ERRORS();
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		GL_ERRORS();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GL_ERRORS();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		GL_ERRORS();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		GL_ERRORS();

		glGenerateMipmap(GL_TEXTURE_2D);
		GL_ERRORS();
		is_first_time = false;
	}
	else {
		// we don't need to remake the texture, just change the data
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, text_size.x, text_size.y, GL_RED, GL_UNSIGNED_BYTE, text_texture.data());
		GL_ERRORS();
		glGenerateMipmap(GL_TEXTURE_2D);
		GL_ERRORS();
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	GL_ERRORS();
}

void PlayMode::Line::render(ColorTextureProgram& color_texture_program, const glm::mat4& court_to_clip) {
	std::vector<Vertex> vertices;

	// TODO: remove function since we only use it once
	auto draw_rectangle = [&vertices](glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
		//draw rectangle as two CCW-oriented triangles:
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.f, 1.f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y-radius.y, 0.0f), color, glm::vec2(1.f, 1.f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(1.f, 0.f));

		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.f, 1.f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(1.f, 0.f));
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.f, 0.f));
	};

	float f = (80.f/2500) * 9.f;
	draw_rectangle(glm::vec2(0.f, 2*ypos*f), glm::vec2(9.f, f), glm::u8vec4(0xff, 0xff, 0xff, 0xff));

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind text texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, current_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the text texture
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);
	

	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.
}



PlayMode::PlayMode() {
	#include "dialog.tree"

		
	lines.emplace_back(color_texture_program, data_path("Inconsolata-Regular.ttf"), 0);
	for (int i = 0; i < 7; i++) {
		lines.emplace_back(color_texture_program, data_path("oxanium/ttf/Oxanium-Regular.ttf"), -i-1);
	}
	for (int i = 8; i < 16; i++) {
		lines.emplace_back(color_texture_program, data_path("Inconsolata-Regular.ttf"), i-7);
	}

	for (int i = 0; i < 8; i++) {
		lines[i].setText("   ");
	}

	for (int i = 8; i < 16; i++) {
		lines[i].setText(stage[15-i]);
	}

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < stage[0].size(); j++) {
			if (stage[i][j] == 'P') {
				player_x = j;
				player_y = i;
				break;
			}
		}
	}


}

// Game crashes if I uncomment this, not sure why
PlayMode::Line::~Line() {
	// glDeleteBuffers(1, &vertex_buffer);
	// vertex_buffer = 0;

	// glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	// vertex_buffer_for_color_texture_program = 0;

	// glDeleteTextures(1, &current_tex);
	// current_tex = 0;

	// hb_buffer_destroy(hb_buffer);
	// hb_font_destroy(hb_font);

	// FT_Done_Face(ft_face);
	// FT_Done_FreeType(ft_library);
}

// these aren't used anyway
PlayMode::~PlayMode() {

	//----- free OpenGL resources -----
	// glDeleteBuffers(1, &vertex_buffer);
	// vertex_buffer = 0;

	// glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	// vertex_buffer_for_color_texture_program = 0;

	// glDeleteTextures(1, &white_tex);
	// white_tex = 0;
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (!free_roam) {

		if (evt.type == SDL_KEYDOWN) {
			if (evt.key.keysym.sym == SDLK_1) {
				if (!pressed_1) {
					pressed_1 = true;
					if (node_map[current_node].questions.size() > 0) {
						current_node = node_map[current_node].questions[0].second;
						node_changed = true;
					}
				}
			}
			else if (evt.key.keysym.sym == SDLK_2) {
				if (!pressed_2) {
					pressed_2 = true;
					if (node_map[current_node].questions.size() > 1) {
						current_node = node_map[current_node].questions[1].second;
						node_changed = true;
					}
				}
			}
			else if (evt.key.keysym.sym == SDLK_3) {
				if (!pressed_3) {
					pressed_3 = true;
					if (node_map[current_node].questions.size() > 2) {
						current_node = node_map[current_node].questions[2].second;
						node_changed = true;
					}
				}
			}
			else if (evt.key.keysym.sym == SDLK_4) {
				if (!pressed_4) {
					pressed_4 = true;
					if (node_map[current_node].questions.size() > 3) {
						current_node = node_map[current_node].questions[3].second;
						node_changed = true;
					}
				}
			}
		}

		else if (evt.type == SDL_KEYUP) {
			if (evt.key.keysym.sym == SDLK_1) {
				pressed_1 = false;
			}
			if (evt.key.keysym.sym == SDLK_2) {
				pressed_2 = false;
			}
			if (evt.key.keysym.sym == SDLK_3) {
				pressed_3 = false;
			}
			if (evt.key.keysym.sym == SDLK_4) {
				pressed_4 = false;
			}
		}	
	}
	else {
		if (evt.type == SDL_KEYDOWN) {
			if (evt.key.keysym.sym == SDLK_w) {
				if (!pressed_w) {
					pressed_w = true;
					move_y = -1;
					move_changed = true;
				}
			}
			else if (evt.key.keysym.sym == SDLK_s) {
				if (!pressed_s) {
					pressed_s = true;
					move_y = 1;
					move_changed = true;
				}
			}
			else if (evt.key.keysym.sym == SDLK_a) {
				if (!pressed_a) {
					pressed_a = true;
					move_x = -1;
					move_changed = true;
				}
			}
			else if (evt.key.keysym.sym == SDLK_d) {
				if (!pressed_d) {
					pressed_d = true;
					move_x = 1;
					move_changed = true;
				}
			}
		}
		else if (evt.type == SDL_KEYUP) {
			if (evt.key.keysym.sym == SDLK_w) {
				pressed_w = false;
			}
			if (evt.key.keysym.sym == SDLK_s) {
				pressed_s = false;
			}
			if (evt.key.keysym.sym == SDLK_a) {
				pressed_a = false;
			}
			if (evt.key.keysym.sym == SDLK_d) {
				pressed_d = false;
			}
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	if (!free_roam) {
		if (node_changed) {
			node_changed = false;
			if (current_node < 12) {
				const auto& node = node_map[current_node];
				lines[0].setText(node.baseline);
				auto current_idx = 1;
				for (const auto& d : node.dialog) {
					lines[current_idx].setText(d);
					current_idx++;
				}

				if (current_idx < 8) {
					lines[current_idx].setText("  ");
					current_idx++;
					if (current_idx < 8) {
						for (const auto& q : node.questions) {
							lines[current_idx].setText(q.first);
							current_idx++;
						}
						if (current_idx < 8) {
							while(current_idx != 8) {
								lines[current_idx].setText("   ");
								current_idx++;
							}
						}
					}
				}
			}
			else if (current_node == 12) {
				lines[0].setText("==== Free Roam ============================================================");
				free_roam = true;
			}
		}
	}
	else {
		if (move_changed) {
			move_changed = false;
			if (move_x == 1) {
				if (player_x < (stage[0].size() - 1) && stage[player_y][player_x + 1] == ' ') {
					stage[player_y][player_x] = ' ';
					player_x += 1;
					stage[player_y][player_x] = 'P';
					lines[15-player_y].setText(stage[player_y]);
					move_x = 0;
				}
			}
			else if (move_x == -1) {
				if (player_x > 0 && stage[player_y][player_x - 1] == ' ') {
					stage[player_y][player_x] = ' ';
					player_x -= 1;
					stage[player_y][player_x] = 'P';
					lines[15-player_y].setText(stage[player_y]);
					move_x = 0;
				}
			}
			else if (move_y == 1) {
				if (player_y < (stage.size() - 1) && stage[player_y + 1][player_x] == ' ') {
					stage[player_y][player_x] = ' ';
					lines[15-player_y].setText(stage[player_y]);
					player_y += 1;
					stage[player_y][player_x] = 'P';
					lines[15-player_y].setText(stage[player_y]);
					move_y = 0;
				}
			}
			else if (move_y == -1) {
				if (player_y > 0 && stage[player_y-1][player_x] == ' ') {
					stage[player_y][player_x] = ' ';
					lines[15-player_y].setText(stage[player_y]);
					player_y -= 1;
					stage[player_y][player_x] = 'P';
					lines[15-player_y].setText(stage[player_y]);
					move_y = 0;
				}
			}
		}
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//some nice colors from the course web page:
	#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0x00000000);
	const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0xf2d2b6ff);
	const glm::u8vec4 shadow_color = HEX_TO_U8VEC4(0xf2ad94ff);
	const std::vector< glm::u8vec4 > trail_colors = {
		HEX_TO_U8VEC4(0xf2ad9488),
		HEX_TO_U8VEC4(0xf2897288),
		HEX_TO_U8VEC4(0xbacac088),
	};
	#undef HEX_TO_U8VEC4

	//other useful drawing constants:
	const float wall_radius = 0.05f;
	const float shadow_offset = 0.07f;
	const float padding = 0.14f; //padding between outside of walls and edge of window

	//---- compute vertices to draw ----

	//------ compute court-to-window transform ------

	 glm::vec2 score_radius = glm::vec2(0.1f, 0.1f);
	//compute area that should be visible:
	glm::vec2 scene_min = glm::vec2(
		-court_radius.x - 2.0f * wall_radius - padding,
		-court_radius.y - 2.0f * wall_radius - padding
	);
	glm::vec2 scene_max = glm::vec2(
		court_radius.x + 2.0f * wall_radius + padding,
		court_radius.y + 2.0f * wall_radius + 3.0f * score_radius.y + padding
	);

	//compute window aspect ratio:
	float aspect = drawable_size.x / float(drawable_size.y);
	//we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

	//compute scale factor for court given that...
	float scale = std::min(
		(2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
		(2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
	);

	glm::vec2 center = 0.5f * (scene_max + scene_min);

	//build matrix that scales and translates appropriately:
	glm::mat4 court_to_clip = glm::mat4(
		glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-center.x * (scale / aspect), -center.y * scale, 0.0f, 1.0f)
	);
	//NOTE: glm matrices are specified in *Column-Major* order,
	// so each line above is specifying a *column* of the matrix(!)

	//also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
	clip_to_court = glm::mat3x2(
		glm::vec2(aspect / scale, 0.0f),
		glm::vec2(0.0f, 1.0f / scale),
		glm::vec2(center.x, center.y)
	);

	//---- actual drawing ----

	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//use alpha blending:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	for (auto& line : lines) {
		line.render(color_texture_program, court_to_clip);
	}
}
