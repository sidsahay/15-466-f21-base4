// Adapted from https://github.com/15-466/15-466-f21-base0

#include "ColorTextureProgram.hpp"

#include "Mode.hpp"
#include "GL.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>


struct Node {
	std::string baseline;
	std::vector<std::string> dialog;
	std::vector<std::pair<std::string, uint64_t>> questions;
};


struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	glm::vec2 court_radius = glm::vec2(7.0f, 5.0f);
	glm::vec2 paddle_radius = glm::vec2(0.2f, 1.0f);
	glm::vec2 ball_radius = glm::vec2(0.2f, 0.2f);

	
	//----- pretty gradient trails -----

	

	//----- opengl assets / helpers ------

	//draw functions will work on vectors of vertices, defined as follows:
	struct Vertex {
		Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_, glm::vec2 const &TexCoord_) :
			Position(Position_), Color(Color_), TexCoord(TexCoord_) { }
		glm::vec3 Position;
		glm::u8vec4 Color;
		glm::vec2 TexCoord;
	};
	static_assert(sizeof(Vertex) == 4*3 + 1*4 + 4*2, "PlayMode::Vertex should be packed");

	// left-aligned line
	// hacky inefficient implementation
	// Currently each has its own set of Harfbuzz and FreeType objects
	// This is so that I can make lines from different fonts and settings (might not actually use this)
	// Lines update (use harfbuzz and freetype to re-render) lazily, so hopefully makes up for some of the inefficiency
	// TODO: change to smart pointers
	// Referenced:
	// https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
	// https://www.freetype.org/freetype2/docs/tutorial/step1.html
	struct Line {
		// harfbuzz and freetype data
		FT_Library ft_library = nullptr;
		FT_Face ft_face = nullptr;

		hb_font_t* hb_font = nullptr;
		hb_buffer_t* hb_buffer = nullptr;
		
		int ypos = 0;

		// basically just repeat what pong did, for each line
		GLuint current_tex = 0;
		GLuint vertex_buffer = 0;
		GLuint vertex_buffer_for_color_texture_program;

		void setText(const std::string& t);
		void render(ColorTextureProgram& program, const glm::mat4& court_to_clip);

		Line(ColorTextureProgram& color_texture_program, const std::string& font_path, int y);
		~Line();
		private:
		bool is_first_time = true;
	};

	std::vector<Line> lines;
	uint64_t current_node = 0;
	bool node_changed = true;

	bool pressed_1 = false;
	bool pressed_2 = false;
	bool pressed_3 = false;
	bool pressed_4 = false;

	bool pressed_w = false;
	bool pressed_s = false;
	bool pressed_a = false;
	bool pressed_d = false;

	bool move_changed = false;

	bool free_roam = false;

	int player_x;
	int player_y;
	int move_x = 0;
	int move_y = 0;

	//Shader program that draws transformed, vertices tinted with vertex colors:
	ColorTextureProgram color_texture_program;

	//Buffer used to hold vertex data during drawing:
	GLuint vertex_buffer = 0;

	//Vertex Array Object that maps buffer locations to color_texture_program attribute locations:
	GLuint vertex_buffer_for_color_texture_program = 0;

	//Solid white texture:
	GLuint white_tex = 0;

	//matrix that maps from clip coordinates to court-space coordinates:
	glm::mat3x2 clip_to_court = glm::mat3x2(1.0f);
	// computed in draw() as the inverse of OBJECT_TO_CLIP
	// (stored here so that the mouse handling code can use it to position the paddle)

};
