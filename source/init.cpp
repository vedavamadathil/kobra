#include "../include/init.hpp"
#include <GLFW/glfw3.h>

// TODO: do a ifndef mercury source dir
#ifndef MERCURY_SOURCE_DIR

#error MERCURY_SOURCE_DIR not defined

#endif

namespace mercury {

// Initial window dimensions
float win_width = 800.0;
float win_height = 600.0;

glm::vec2 transform(const glm::vec2 &in)
{
	return glm::vec2 {
		(in.x - win_width/2)/(win_width/2),
		-(in.y - win_height/2)/(win_height/2)
	};
}

// Character static variables
Shader Char::shader;

// Character map
std::unordered_map <char, Char> cmap;

// Window size change callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// Private static methods
static GLFWwindow *_init_glfw()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__

	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "Mercury", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		exit(-1);
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// Now initialize glad
	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		exit(-1);
	}

	// OpenGL options
	// TODO: disable only in debug mode
	// glEnable(GL_CULL_FACE);


        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	return window;
}

void load_fonts()
{
	// Load default font
	FT_Library ft;
	if (FT_Init_FreeType(&ft)) {
		std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
		exit(-1);
	}

	FT_Face face;
	int ret;
	if ((ret = FT_New_Face(ft, MERCURY_SOURCE_DIR "/resources/fonts/arial.ttf", 0, &face))) {
		std::cout << "ERROR::FREETYPE: Failed to load font (" << ret << ")" << std::endl;
		exit(-1);
	}

	FT_Set_Pixel_Sizes(face, 0, 48);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
	for (unsigned char c = 0; c < 128; c++) {
		// load character glyph
		if (FT_Load_Char(face, c, FT_LOAD_RENDER))
		{
			std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
			continue;
		}
		// generate texture
		unsigned int texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RED,
				face->glyph->bitmap.width,
				face->glyph->bitmap.rows,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				face->glyph->bitmap.buffer
		);
		// set texture options
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// now store character for later use
		Char character = {
			texture,
			glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
			glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
			(unsigned int) face->glyph->advance.x
		};

		cmap.insert({c, character});
	}

	// Release resources
	FT_Done_Face(face);
	FT_Done_FreeType(ft);

	// Create the text shader
	Char::shader = Shader(
		MERCURY_SOURCE_DIR "/resources/shaders/font_shader.vs",
		MERCURY_SOURCE_DIR "/resources/shaders/font_shader.fs"
	);
}

// TODO: should return error code
// TODO: split into separate functions
GLFWwindow *init()
{
	// Very first thing
	GLFWwindow *window = _init_glfw();

	load_fonts();

	// Return the window on success
	return window;
}

}