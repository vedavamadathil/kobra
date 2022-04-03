#ifndef KOBRA_RT_LAYER_H_
#define KOBRA_RT_LAYER_H_

// Standard headers
#include <vector>

// Engine headers
#include "../../shaders/rt/mesh_bindings.h"
#include "../app.hpp"
#include "../backend.hpp"
#include "../bbox.hpp"
#include "../bvh.hpp"
#include "../camera.hpp"
#include "../layer.hpp"
#include "../logger.hpp"
#include "rt.hpp"

namespace kobra {

namespace rt {

class Layer : public kobra::Layer <rt::_element> {
	// Private aliases
	using DSLBinding = VkDescriptorSetLayoutBinding;
	using DSLBindings = std::vector <DSLBinding>;

	// All of the layer's cameras
	std::vector <Camera>	_cameras;

	// Active camera
	Camera			*_active_camera;

	// Vulkan context
	Vulkan::Context		_context;

	// Descriptor pool
	VkDescriptorPool	_descriptor_pool;

	// Swapchain extent
	VkExtent2D		_extent;

	// Render pass
	VkRenderPass		_render_pass;

	// Pipelines
	struct {
		Vulkan::Pipeline mesh;
		Vulkan::Pipeline postproc;
	} _pipelines;

	// Descriptor set bindings
	static const DSLBindings _mesh_compute_bindings;
	static const DSLBindings _postproc_bindings;

	VkDescriptorSetLayout	_mesh_dsl;
	VkDescriptorSetLayout	_postproc_dsl;

	// Descriptor sets
	VkDescriptorSet		_mesh_ds;
	VkDescriptorSet		_postproc_ds;

	// Barrier
	// VkImageMemoryBarrier	_barrier;

	// Initialize mesh compute pipeline
	void _init_mesh_compute_pipeline();
	void _init_postproc_pipeline(const Vulkan::Swapchain &);

	// Initialize all pipelines
	void _init_pipelines(const Vulkan::Swapchain &swapchain) {
		// First, create the DSLs
		_mesh_dsl = _context.vk->make_descriptor_set_layout(
			_context.device,
			_mesh_compute_bindings
		);

		_postproc_dsl = _context.vk->make_descriptor_set_layout(
			_context.device,
			_postproc_bindings
		);

		// Then, create the descriptor sets
		_mesh_ds = _context.vk->make_descriptor_set(
			_context.device,
			_descriptor_pool,
			_mesh_dsl
		);

		_postproc_ds = _context.vk->make_descriptor_set(
			_context.device,
			_descriptor_pool,
			_postproc_dsl
		);

		// All pipelines
		_init_mesh_compute_pipeline();
		_init_postproc_pipeline(swapchain);
	}

	// Data
	BufferManager <uint>	_pixels;
	Buffer4f		_vertices;
	Buffer4f		_triangles;
	Buffer4f		_materials;
	Buffer4f		_lights;		// TODO: make as a 4u buffer, less casting overall in the shader
	BufferManager <uint>	_light_indices;

	Buffer4m		_transforms;

	// Normal samplers
	Sampler			_albedo_sampler;
	Sampler			_normal_sampler;

	// BVH
	BVH			_bvh;

	// Batching variables
	int			_xbatch = 0;
	int			_ybatch = 0;

	int			_xbatch_size = 50;
	int			_ybatch_size = 50;

	// Get list of bboxes for each triangle
	std::vector <BoundingBox> _get_bboxes() const {
		std::vector <BoundingBox> bboxes;
		bboxes.reserve(_triangles.size());

		const auto &vertices = _vertices.vector();
		const auto &triangles = _triangles.vector();

		for (size_t i = 0; i < _triangles.push_size(); i++) {
			const auto &triangle = triangles[i];

			float ia = triangle.data.x;
			float ib = triangle.data.y;
			float ic = triangle.data.z;
			float id = triangle.data.w;

			uint a = *(reinterpret_cast <uint *> (&ia));
			uint b = *(reinterpret_cast <uint *> (&ib));
			uint c = *(reinterpret_cast <uint *> (&ic));
			uint d = *(reinterpret_cast <uint *> (&id));

			// If a == b == c, its a sphere
			if (a == b && b == c) {
				glm::vec4 center = vertices[2 * a].data;
				float radius = center.w;

				glm::vec4 min = center - glm::vec4(radius);
				glm::vec4 max = center + glm::vec4(radius);

				bboxes.push_back(BoundingBox {min, max});
			} else {
				glm::vec4 va = vertices[2 * a].data;
				glm::vec4 vb = vertices[2 * b].data;
				glm::vec4 vc = vertices[2 * c].data;

				glm::vec4 min = glm::min(va, glm::min(vb, vc));
				glm::vec4 max = glm::max(va, glm::max(vb, vc));

				bboxes.push_back(BoundingBox {min, max});
			}
		}

		return bboxes;
	}
public:
	// Default constructor
	Layer() = default;

	// Constructor
	Layer(const App::Window &wctx)
			: _context(wctx.context),
			_extent(wctx.swapchain.extent),
			_descriptor_pool(wctx.descriptor_pool) {
		// Create the render pass
		// TODO: context method
		_render_pass = _context.vk->make_render_pass(
			_context.phdev,
			_context.device,
			wctx.swapchain,
			VK_ATTACHMENT_LOAD_OP_LOAD,
			VK_ATTACHMENT_STORE_OP_STORE,
			false
		);

		// Initialize pipelines
		_init_pipelines(wctx.swapchain);

		// Allocate buffers
		size_t pixels = wctx.swapchain.extent.width
			* wctx.swapchain.extent.height;

		BFM_Settings pixel_settings {
			.size = pixels,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.usage_type = BFM_READ_ONLY
		};

		BFM_Settings viewport_settings {
			.size = 2,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.usage_type = BFM_WRITE_ONLY
		};

		BFM_Settings write_only_settings {
			.size = 1024,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.usage_type = BFM_WRITE_ONLY
		};

		_pixels = BufferManager <uint> (_context, pixel_settings);

		_vertices = Buffer4f(_context, write_only_settings);
		_triangles = Buffer4f(_context, write_only_settings);
		_materials = Buffer4f(_context, write_only_settings);
		_lights = Buffer4f(_context, write_only_settings);
		_light_indices = BufferManager <uint> (_context, write_only_settings);
		_transforms = Buffer4m(_context, write_only_settings);

		KOBRA_LOG_FUNC(notify) << "Initialized rt::Layer, mesh.plaout = "
			<< _pipelines.mesh.layout << "\n";

		// Bind to descriptor sets
		_pixels.bind(_mesh_ds, MESH_BINDING_PIXELS);
		_pixels.bind(_postproc_ds, MESH_BINDING_PIXELS);

		// Textures
		// TODO: should be a static method
		/* Texture tex1 = load_image_texture(
			"/home/venki/downloads/quixel/Nature_Rock_vizvcbn_2K_3d_ms/"
				"vizvcbn_2K_Normal_LOD0.jpg",
			4
		); */
		Texture tex1 {
			.width = 1024,
			.height = 1024,
			.channels = 4,
		};

		KOBRA_LOG_FILE(warn) << "tex channels = " << tex1.channels << "\n";

		TexturePacket tp = make_texture(
			_context, wctx.command_pool,
			tex1,
			VK_FORMAT_R8G8B8A8_SRGB, // TODO: should match the # of channels in texture
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		tp.transition_manual(_context, wctx.command_pool,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);

		_normal_sampler = Sampler(_context, tp);
		_normal_sampler.bind(_mesh_ds, MESH_BINDING_NORMAL_MAPS);

		KOBRA_LOG_FILE(notify) << "Loaded texture: " << tex1.width << "x"
			<< tex1.height << " --> #bytes = " << tex1.data.size() << "\n";

		// Albedo
		// Texture tex2 = load_image_texture("resources/wood_floor_albedo.jpg", 4);
		Texture tex2 = load_image_texture("resources/wood_floor_albedo.jpg", 4);

		TexturePacket tp2 = make_texture(
			_context, wctx.command_pool,
			tex2,
			VK_FORMAT_R8G8B8A8_UNORM, // TODO: should match the # of channels in texture
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		tp2.transition_manual(_context, wctx.command_pool,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);

		_albedo_sampler = Sampler(_context, tp2);
		_albedo_sampler.bind(_mesh_ds, MESH_BINDING_ALBEDO);
	}

	// Adding elements
	// TODO: element actions from base class?
	void add_do(const ptr &e) override {
		std::cout << "ADDED ELEMENT\n";
		LatchingPacket lp {
			.vertices = &_vertices,
			.triangles = &_triangles,
			.materials = &_materials,
			.transforms = &_transforms,
			.lights = &_lights,
			.light_indices = &_light_indices
		};

		e->latch(lp, _elements.size());

		// Flush the vertices and triangles
		_vertices.sync_upload();
		_triangles.sync_upload();
		_materials.sync_upload();
		_transforms.sync_upload();
		_lights.sync_upload();
		_light_indices.sync_upload();

		// Rebind to descriptor sets
		_vertices.bind(_mesh_ds, MESH_BINDING_VERTICES);
		_triangles.bind(_mesh_ds, MESH_BINDING_TRIANGLES);
		_materials.bind(_mesh_ds, MESH_BINDING_MATERIALS);
		_transforms.bind(_mesh_ds, MESH_BINDING_TRANSFORMS);
		_lights.bind(_mesh_ds, MESH_BINDING_LIGHTS);
		_light_indices.bind(_mesh_ds, MESH_BINDING_LIGHT_INDICES);

		// Update the BVH
		_bvh = BVH(_context, _get_bboxes());

		// Rebind to descriptor sets
		_bvh.bind(_mesh_ds, MESH_BINDING_BVH);
	}

	// Number of triangles
	size_t triangle_count() const {
		return _triangles.push_size();
	}

	// Number of cameras
	size_t camera_count() const {
		return _cameras.size();
	}

	// Add a camera to the layer
	void add_camera(const Camera &camera) {
		_cameras.push_back(camera);
	}

	// Active camera
	Camera *active_camera() {
		return _active_camera;
	}

	// Activate a camera
	Camera *activate_camera(size_t index) {
		if (index < _cameras.size()) {
			_active_camera = &_cameras[index];
		} else {
			KOBRA_LOG_FUNC(warn) << "Camera index out of range ["
				<< index << "/" << _cameras.size() << "]";
		}

		return _active_camera;
	}

	// Render
	void render(const VkCommandBuffer &cmd, const VkFramebuffer &framebuffer) {
		///////////////////////////
		// Mesh compute pipeline //
		///////////////////////////

		// TODO: context method
		vkCmdBindPipeline(cmd,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			_pipelines.mesh.pipeline
		);

		// Prepare push constants
		PushConstants pc {
			.width = _extent.width,
			.height = _extent.height,

			.xoffset = uint(_xbatch * _xbatch_size),
			.yoffset = uint(_ybatch * _ybatch_size),

			.triangles = (uint) _triangles.push_size(),
			.lights = (uint) _light_indices.push_size(),

			// TODO: still unable to do large number of samples
			.samples_per_pixel = 25,
			.samples_per_light = 64,

			.camera_position = _active_camera->transform.position,
			.camera_forward = _active_camera->transform.forward(),
			.camera_up = _active_camera->transform.up(),
			.camera_right = _active_camera->transform.right(),

			.camera_tunings = glm::vec4 {
				active_camera()->tunings.scale,
				active_camera()->tunings.aspect,
				0, 0
			}
		};

		// Bind descriptor set
		vkCmdBindDescriptorSets(cmd,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			_pipelines.mesh.layout,
			0, 1, &_mesh_ds,
			0, nullptr
		);

		// Push constants
		vkCmdPushConstants(cmd,
			_pipelines.mesh.layout,
			VK_SHADER_STAGE_COMPUTE_BIT,
			0, sizeof(PushConstants), &pc
		);

		// Dispatch the compute shader
		vkCmdDispatch(cmd,
			_xbatch_size,
			_ybatch_size,
			1
		);

		// Update batch offsets
		_xbatch += 1;
		if (_xbatch * _xbatch_size >= _extent.width) {
			_xbatch = 0;
			_ybatch += 1;
		}

		if (_ybatch * _ybatch_size >= _extent.height) {
			_ybatch = 0;
			_xbatch = 0;
		}

		//////////////////////////////
		// Post-processing pipeline //
		//////////////////////////////

		vkCmdBindPipeline(cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			_pipelines.postproc.pipeline
		);

		// Bind descriptor set
		vkCmdBindDescriptorSets(cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			_pipelines.postproc.layout,
			0, 1, &_postproc_ds,
			0, nullptr
		);

		// Push constants
		PC_Viewport pc_vp {
			.width = _extent.width,
			.height = _extent.height
		};

		vkCmdPushConstants(cmd,
			_pipelines.postproc.layout,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(PC_Viewport), &pc_vp
		);

		// Begin render pass
		// TODO: context method
		VkRenderPassBeginInfo rp_info {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = _render_pass,
			.framebuffer = framebuffer,
			.renderArea = {
				.offset = {0, 0},
				.extent = _extent
			},
			.clearValueCount = 0,
			.pClearValues = nullptr
		};

		vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdDraw(cmd, 6, 1, 0, 0);
		vkCmdEndRenderPass(cmd);
	}
};

}

}

#endif
