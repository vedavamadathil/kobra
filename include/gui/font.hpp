#ifndef FONT_H_
#define FONT_H_

// Standard headers
#include <array>
#include <string>
#include <unordered_map>

// FreeType headers
#include <ft2build.h>
#include <vulkan/vulkan_core.h>
#include FT_FREETYPE_H
#include FT_BBOX_H
#include FT_OUTLINE_H

// Engine headers
#include "../common.hpp"
#include "../logger.hpp"
#include "gui.hpp"

namespace mercury {

namespace gui {

// Glyph outline structure
struct GlyphOutline {
	// Glyph metrics
	struct Metrics {
		float xbear;
		float ybear;

		float width;
		float height;
	} metrics;

	// Previously inserted glyph point
	glm::vec2 prev;

	// Store outline data as a list
	// of quadratic bezier curves
	BufferManager <glm::vec2> outline;

	// Default constructor
	GlyphOutline() {}

	// Constructor takes bounding box
	// TODO: first vec2 contains number of curves
	GlyphOutline(const Vulkan::Context &ctx, const Metrics &m) : metrics(m) {
		// Allocate space for outline data
		BFM_Settings outline_settings {
			.size = 100,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.usage_type = BFM_WRITE_ONLY
		};

		outline = BufferManager <glm::vec2> (ctx, outline_settings);
	}

	// Push a new point to the list
	void push(const glm::vec2 &p) {
		outline.push_back(p);
		prev = p;
	}

	glm::vec2 convert(const FT_Vector *point) {
		// Normalize coordinates
		float x = (point->x - metrics.xbear) / metrics.width;
		float y = (point->y + metrics.height - metrics.ybear) / metrics.height;
		return glm::vec2 {x, y/2};
	}

	// Bind buffer to descriptor set
	void bind(const VkDescriptorSet &ds, uint32_t binding) {
		outline.bind(ds, binding);
	}

	// Sync and upload data to GPU
	void upload() {
		// Insert size vector
		size_t size = outline.push_size();
		outline.push_front(glm::vec2 {
			static_cast <float> (size),
			0.0f
		});

		outline.sync_size();
		outline.upload();
	}

	// TODO: debugging only
	// Dump outline data to console
	void dump() const {
		Logger::ok() << "Glyph outline: ";
		for (const auto &p : outline.vector())
			Logger::plain() << "(" << p.x << ", " << p.y << "), ";
		Logger::plain() << std::endl;
	}
};

// Glyph structure
// TODO: text class will hold shaders and stuff
class Glyph {
public:
	// Vertex and buffer type
	struct Vertex {
		glm::vec4 bounds;

		// Get vertex binding description
		static VertexBinding vertex_binding() {
			return VertexBinding {
				.binding = 0,
				.stride = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			};
		}

		// Get vertex attribute descriptions
		static std::array <VertexAttribute, 1> vertex_attributes() {
			return std::array <VertexAttribute, 1> {
				VertexAttribute {
					.location = 0,
					.binding = 0,
					.format = VK_FORMAT_R32G32B32A32_SFLOAT,
					.offset = offsetof(Vertex, bounds)
				},
			};
		}
	};

	using VertexBuffer = BufferManager <Vertex>;
private:
	// Vertex and index data
	glm::vec4	_bounds;
	glm::vec3	_color	= glm::vec3 {1.0};
public:
	// Constructor
	Glyph() {}
	Glyph(glm::vec4 bounds) : _bounds(bounds) {}

	// Render the glyph
	// TODO: render method or upload method (instacing)?
	void upload(VertexBuffer &vb) {
		std::array <Vertex, 6> vertices {
			Vertex {_bounds},
			Vertex {_bounds},
			Vertex {_bounds},
			Vertex {_bounds},
			Vertex {_bounds},
			Vertex {_bounds}
		};

		vb.push_back(vertices);
	}

	// Static buffer properties
	static constexpr BFM_Settings vb_settings {
		.size = 1024,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.usage_type = BFM_WRITE_ONLY
	};

	// TODO: remove
	static constexpr BFM_Settings ib_settings {
		.size = 1024,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.usage_type = BFM_WRITE_ONLY
	};

	// Descriptor set for shader
	static constexpr VkDescriptorSetLayoutBinding glyph_dsl {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	// Make descriptor set layout
	static VkDescriptorSetLayout make_glyph_dsl(const Vulkan::Context &ctx) {
		static VkDescriptorSetLayout dsl = VK_NULL_HANDLE;

		if (dsl != VK_NULL_HANDLE)
			return dsl;

		// Create layout if not created
		dsl = ctx.vk->make_descriptor_set_layout(
			ctx.device,
			{ glyph_dsl }
		);

		return dsl;
	}

	// Make descriptor set
	static VkDescriptorSet make_glyph_ds(const Vulkan::Context &ctx, const VkDescriptorPool &pool) {
		static VkDescriptorSet ds = VK_NULL_HANDLE;

		if (ds != VK_NULL_HANDLE)
			return ds;

		// Create descriptor set
		ds = ctx.vk->make_descriptor_set(
			ctx.device,
			pool,
			make_glyph_dsl(ctx)
		);

		return ds;
	}
};

// Font class holds information
//	about a single font
class Font {
	// Font data
	std::unordered_map <char, GlyphOutline> _glyphs;

	// Check FreeType error
	void check_error(FT_Error error) const {
		if (error) {
			Logger::error() << "FreeType error: "
				<< error << std::endl;
			throw -1;
		}
	}

	// Outline processors
	static FT_Error _outline_move_to(const FT_Vector *to, void *user) {
		GlyphOutline *outline = (GlyphOutline *) user;
		outline->push({0, 0});
		outline->push(outline->convert(to));
		return 0;
	}

	static FT_Error _outline_line_to(const FT_Vector *to, void *user) {
		GlyphOutline *outline = (GlyphOutline *) user;
		glm::vec2 prev = outline->prev;
		glm::vec2 curr = outline->convert(to);
		// Lerped point for a line
		glm::vec2 lerped = glm::mix(prev, curr, 0.5f);
		// outline->push((prev + curr) / 2.0f);
		outline->push(lerped);
		outline->push(curr);
		return 0;
	}

	static FT_Error _outline_conic_to(const FT_Vector *control, const FT_Vector *to, void *user) {
		GlyphOutline *outline = (GlyphOutline *) user;
		outline->push(outline->convert(control));
		outline->push(outline->convert(to));
		return 0;
	}

	static FT_Error _outline_cubic_to(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user) {
		GlyphOutline *outline = (GlyphOutline *) user;
		/* outline->push(control1);
		outline->push(control2);
		outline->push(to); */
		Logger::error() << "IGNORED CUBIC TO" << std::endl;
		return 0;
	}

	// Load FreeType library
	void load_font(const Vulkan::Context &ctx, const std::string &file) {
		// Load library
		FT_Library library;
		check_error(FT_Init_FreeType(&library));

		// Load font
		FT_Face face;
		check_error(FT_New_Face(library, file.c_str(), 0, &face));

		// Set font size
		check_error(FT_Set_Char_Size(face, 0, 1000 * 64, 96, 96));

		// Process
		size_t total_points = 0;
		size_t total_cells = 0;

		for (size_t i = 0; i < 96; i++) {
			char c = i + ' ';
			std::cout << "Processing character '" << c << "'" << std::endl;

			// Get outline
			FT_UInt glyph_index = FT_Get_Char_Index(face, c);
			check_error(FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT));

			// Process outline
			FT_Outline_Funcs funcs = {
				.move_to = _outline_move_to,
				.line_to = _outline_line_to,
				.conic_to = _outline_conic_to,
				.cubic_to = _outline_cubic_to,
			};

			// Get bounding box
			GlyphOutline outline = {ctx, {
				.xbear = (float) face->glyph->bitmap_left,
				.ybear = (float) face->glyph->bitmap_top,
				.width = (float) face->glyph->metrics.width,
				.height = (float) face->glyph->metrics.height
			}};

			// Get number of curves
			FT_Outline_Decompose(&face->glyph->outline, &funcs, &outline);
			outline.upload();
			outline.dump();

			// Add to glyphs
			_glyphs[c] = outline;
		}
	}
public:
	Font() {}
	Font(const Vulkan::Context &ctx, const std::string &file) {
		// Check that the file exists
		if (!file_exists(file)) {
			Logger::error("Font file not found: " + file);
			throw -1;
		}

		// Load font
		load_font(ctx, file);
	}

	// Retrieve glyph
	const GlyphOutline &operator[](char c) const {
		auto it = _glyphs.find(c);
		if (it == _glyphs.end()) {
			Logger::error() << "Glyph not found: " << c << std::endl;
			throw -1;
		}

		return it->second;
	}
};

}

}

#endif
