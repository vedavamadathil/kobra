// Standard headers
#include <iostream>

// More Vulkan stuff
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

#define KOBRA_VALIDATION_LAYERS
// #define KOBRA_VALIDATION_ERROR_ONLY

// Engine headers
#include "../include/backend.hpp"
#include "../include/mesh.hpp"
#include "../include/camera.hpp"
#include "../shaders/raster/bindings.h"

using namespace kobra;

// Camera
Camera camera = Camera {
	Transform {
		{0, 0, 5},
		{0, 0, 0}
	},

	Tunings { 45.0f, 800, 800 }
};


// Mouse movement
void mouse_movement(GLFWwindow *, double xpos, double ypos)
{
	static const float sensitivity = 0.0005f;

	static bool first_movement = true;

	static float px = 0.0f;
	static float py = 0.0f;

	static float yaw = 0.0f;
	static float pitch = 0.0f;

	// Deltas and directions
	float dx = xpos - px;
	float dy = ypos - py;

	yaw -= dx * sensitivity;
	pitch -= dy * sensitivity;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	camera.transform.rotation.x = pitch;
	camera.transform.rotation.y = yaw;

	px = xpos;
	py = ypos;

	std::cout << "Pitch: " << pitch << " Yaw: " << yaw << std::endl;
}

// Key callback
void keyboard_input(GLFWwindow *window, int key, int, int action, int)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// Input handling
void input_handling(GLFWwindow *window)
{
	float speed = 0.1f;

	glm::vec3 forward = camera.transform.forward();
	glm::vec3 right = camera.transform.right();
	glm::vec3 up = camera.transform.up();

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.transform.move(forward * speed);
	else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.transform.move(-forward * speed);

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.transform.move(-right * speed);
	else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.transform.move(right * speed);

	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		camera.transform.move(up * speed);
	else if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		camera.transform.move(-up * speed);

	std::cout << "Position: " << camera.transform.position.x << " "
		<< camera.transform.position.y << " "
		<< camera.transform.position.z << std::endl;
}

// Present an image
void present_swapchain_image(const Swapchain &swapchain, uint32_t &image_index,
		const vk::raii::Queue queue,
		const vk::raii::Semaphore &signal_semaphore)
{
	vk::PresentInfoKHR present_info {
		*signal_semaphore,
		*swapchain.swapchain,
		image_index
	};

	vk::Result result = queue.presentKHR(present_info);

	KOBRA_ASSERT(
		result == vk::Result::eSuccess,
		"Failed to present image (result = " + vk::to_string(result) + ")"
	);
}

int main()
{
	// Choosing physical device
	auto window = Window("Vulkan RT", {1000, 1000});

	// GLFW callbacks
	// TODO: window methods
	glfwSetCursorPosCallback(window.handle, mouse_movement);
	glfwSetKeyCallback(window.handle, keyboard_input);

	// Disable cursor
	glfwSetInputMode(window.handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Create a surface
	vk::raii::SurfaceKHR surface = make_surface(window);

	auto extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,

		/* For raytracing
		"VK_KHR_spirv_1_4",
		"VK_KHR_shader_float_controls",
		"VK_KHR_ray_tracing_pipeline",
		"VK_KHR_acceleration_structure",
		"VK_EXT_descriptor_indexing",
		"VK_KHR_maintenance3",
		"VK_KHR_buffer_device_address",
		"VK_KHR_deferred_host_operations", */
	};

	auto predicate = [&extensions](const vk::raii::PhysicalDevice &dev) {
		return physical_device_able(dev, extensions);
	};

	auto phdev = pick_physical_device(predicate);

	std::cout << "Chosen device: " << phdev.getProperties().deviceName << std::endl;
	std::cout << "\tgraphics queue family: " << find_graphics_queue_family(phdev) << std::endl;
	std::cout << "\tpresent queue family: " << find_present_queue_family(phdev, surface) << std::endl;

	// Verification and creating a logical device
	auto queue_family = find_queue_families(phdev, surface);
	std::cout << "\tqueue family (G): " << queue_family.graphics << std::endl;
	std::cout << "\tqueue family (P): " << queue_family.present << std::endl;

	auto queue_families = phdev.getQueueFamilyProperties();

	std::cout <<"Queue families:" << std::endl;
	for (uint32_t i = 0; i < queue_families.size(); i++) {
		auto flags = queue_families[i].queueFlags;
		std::cout << "\tqueue family " << i << ": ";
		if (flags & vk::QueueFlagBits::eGraphics)
			std::cout << "Graphics ";
		if (flags & vk::QueueFlagBits::eCompute)
			std::cout << "Compute ";
		if (flags & vk::QueueFlagBits::eTransfer)
			std::cout << "Transfer ";
		if (flags & vk::QueueFlagBits::eSparseBinding)
			std::cout << "SparseBinding ";

		// Count
		std::cout <<" [" << queue_families[i].queueCount << "]\n";
	}

	auto device = make_device(phdev, queue_family, extensions);

	// Command pool and buffer
	auto command_pool = vk::raii::CommandPool {
		device,
		{
			vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			queue_family.graphics
		}
	};

	// TODO: overload function for this
	vk::raii::CommandBuffer command_buffers[2] {
		make_command_buffer(device, command_pool),
		make_command_buffer(device, command_pool)
	};

	// Queues
	auto graphics_queue = vk::raii::Queue { device, queue_family.graphics, 0 };
	auto present_queue = vk::raii::Queue { device, queue_family.present, 0 };

	// Swapchain
	auto swapchain = Swapchain {phdev, device, surface, window.extent, queue_family};

	// Depth buffer and render pass
	auto depth_buffer = DepthBuffer {phdev, device, vk::Format::eD32Sfloat, window.extent};
	auto render_pass = make_render_pass(device, swapchain.format, depth_buffer.format);

	auto framebuffers = make_framebuffers(
		device, render_pass,
		swapchain.image_views,
		&depth_buffer.view,
		window.extent
	);

	// Box mesh
	auto box_mesh = Mesh::make_box({0, 0, 0}, {1, 1, 1});

	// Allocate vertex and index buffer
	auto vertex_buffer = BufferData(phdev, device,
		box_mesh.vertices().size() * sizeof(Vertex),
		vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible
			| vk::MemoryPropertyFlagBits::eHostCoherent
	);

	auto index_buffer = BufferData(phdev, device,
		box_mesh.indices().size() * sizeof(uint32_t),
		vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible
			| vk::MemoryPropertyFlagBits::eHostCoherent
	);

	// Fill with data
	vertex_buffer.upload(box_mesh.vertices());
	index_buffer.upload(box_mesh.indices());

	// Load shaders
	// TODO: compile function for shaders
	auto vertex = make_shader_module(device, "shaders/bin/raster/vertex.spv");
	auto fragment = make_shader_module(device, "shaders/bin/raster/color_frag.spv");

	// Descriptor set layout
	auto dsl = make_descriptor_set_layout(device, {});

	// Push constant (glm::mat4 MVP)
	auto pcl = vk::PushConstantRange {
		vk::ShaderStageFlagBits::eVertex,
		0, sizeof(glm::mat4)
	};

	auto ppl = vk::raii::PipelineLayout {device, {{}, *dsl, pcl}};
	auto ppl_cache = vk::raii::PipelineCache {device, vk::PipelineCacheCreateInfo()};

	// Vertex input binding and attributes
	auto vertex_input_binding = vk::VertexInputBindingDescription {
		0, sizeof(Vertex),
		vk::VertexInputRate::eVertex
	};

	auto _vertex_input_attributes = Vertex::vertex_attributes();

	// Convert to vk::VertexInputAttributeDescription
	auto vertex_input_attributes = std::vector <vk::VertexInputAttributeDescription> {};
	for (auto &attr : _vertex_input_attributes) {
		vk::Format format;

		// Convert from VkFormat to vk::Format
		if (attr.format == VK_FORMAT_R32G32B32_SFLOAT)
			format = vk::Format::eR32G32B32Sfloat;
		else if (attr.format == VK_FORMAT_R32G32B32A32_SFLOAT)
			format = vk::Format::eR32G32B32A32Sfloat;
		else if (attr.format == VK_FORMAT_R32G32_SFLOAT)
			format = vk::Format::eR32G32Sfloat;

		vertex_input_attributes.push_back(
			vk::VertexInputAttributeDescription {
				attr.location,
				attr.binding,
				format,
				attr.offset
			}
		);
	}

	// Create the graphics pipeline
	auto grp_info = GraphicsPipelineInfo {
		.device = device,
		.render_pass = render_pass,

		.vertex_shader = vertex,
		.fragment_shader = fragment,

		.vertex_binding = vertex_input_binding,
		.vertex_attributes = vertex_input_attributes,

		.pipeline_layout = ppl,
		.pipeline_cache = ppl_cache,
	};

	auto pipeline = make_graphics_pipeline(grp_info);

	// Semaphores
	vk::raii::Semaphore image_available[2] {
		vk::raii::Semaphore {device, vk::SemaphoreCreateInfo {}},
		vk::raii::Semaphore {device, vk::SemaphoreCreateInfo {}},
	};

	vk::raii::Semaphore render_finished[2] {
		vk::raii::Semaphore {device, vk::SemaphoreCreateInfo {}},
		vk::raii::Semaphore {device, vk::SemaphoreCreateInfo {}},
	};

	// Other variables
	vk::Result result;
	uint32_t image_index;

	// Record the command buffer
	auto record = [&](const vk::raii::CommandBuffer &command_buffer) {
		command_buffer.begin({});

		// Set the viewport and scissor
		auto viewport = vk::Viewport {
			0.0f, 0.0f,
			static_cast <float> (window.extent.width),
			static_cast <float> (window.extent.height),
			0.0f, 1.0f
		};

		command_buffer.setViewport(0, viewport);
		command_buffer.setScissor(0, vk::Rect2D {
			vk::Offset2D {0, 0},
			window.extent
		});

		// Push constants
		glm::mat4 model = glm::mat4(1.0f);
		glm::mat4 view = camera.view();
		glm::mat4 proj = camera.perspective();

		std::array <glm::mat4, 1> mvp = {proj * view * model};

		command_buffer.pushConstants <glm::mat4>
			(*ppl, vk::ShaderStageFlagBits::eVertex, 0, mvp);

		// Start the render pass
		std::array <vk::ClearValue, 2> clear_values = {
			vk::ClearValue {
				vk::ClearColorValue {
					std::array <float, 4> {0.0f, 0.0f, 0.0f, 1.0f}
				}
			},
			vk::ClearValue {
				vk::ClearDepthStencilValue {
					1.0f, 0
				}
			}
		};

		command_buffer.beginRenderPass(
			vk::RenderPassBeginInfo {
				*render_pass,
				*framebuffers[image_index],
				vk::Rect2D {
					vk::Offset2D {0, 0},
					window.extent
				},
				static_cast <uint32_t> (clear_values.size()),
				clear_values.data()
			},
			vk::SubpassContents::eInline
		);

		// Bind the pipeline
		command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

		// Bind the cube and draw it
		command_buffer.bindVertexBuffers(0, *vertex_buffer.buffer, {0});
		command_buffer.bindIndexBuffer(*index_buffer.buffer, 0, vk::IndexType::eUint16);
		command_buffer.drawIndexed(
			static_cast <uint32_t> (box_mesh.indices().size()),
			1, 0, 0, 0
		);

		// End the render pass
		command_buffer.endRenderPass();

		// End
		command_buffer.end();
	};

	// Fences for the command buffer
	// auto fence = vk::raii::Fence {device, vk::FenceCreateInfo()};
	vk::raii::Fence images_in_flight[2] {
		vk::raii::Fence {device, nullptr},
		vk::raii::Fence {device, nullptr}
	};

	vk::raii::Fence in_flight_fences[2] {
		vk::raii::Fence {device, vk::FenceCreateInfo {vk::FenceCreateFlagBits::eSignaled}},
		vk::raii::Fence {device, vk::FenceCreateInfo {vk::FenceCreateFlagBits::eSignaled}}
	};

	// Submit the command buffer
	vk::PipelineStageFlags stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	// Present the image
	// present_swapchain_image(swapchain, present_queue, image_index);

	// Present while valid window
	uint32_t frame_index = 0;
	while (!glfwWindowShouldClose(window.handle)) {
		std::cout << "==============================\n";
		std::cout <<"Waiting for fence...\n";
		result = device.waitForFences(
			*in_flight_fences[frame_index],
			true,
			std::numeric_limits <uint64_t> ::max()
		);

		KOBRA_ASSERT(result == vk::Result::eSuccess, "Failed to wait for fence");

		// Grab the next image from the swapchain
		std::tie(result, image_index) = swapchain.swapchain.acquireNextImage(
			std::numeric_limits <uint64_t> ::max(),
			*image_available[frame_index]
		);

		KOBRA_ASSERT(result == vk::Result::eSuccess, "Failed to acquire next image");

		std::cout << "Acquired image " << image_index << "\n";

		// Check if the image is being used by the current frame
		if (*images_in_flight[image_index] != vk::Fence {nullptr}) {
			std::cout <<"Waiting for image...\n";
			result = device.waitForFences(
				*images_in_flight[image_index],
				true,
				std::numeric_limits <uint64_t> ::max()
			);

			KOBRA_ASSERT(result == vk::Result::eSuccess, "Failed to wait for fence");
		}

		// Mark the image as in use
		std::cout << "\nin_flight_fence[" << frame_index << "] = " << *in_flight_fences[frame_index] << "\n";
		images_in_flight[image_index] = std::move(in_flight_fences[frame_index]);
		std::cout << "Marking image as in use...\n";

		record(command_buffers[image_index]);
		std::cout << "Recording command buffer...\n";

		vk::SubmitInfo submit_info {
			1, &*image_available[frame_index],
			&stage_flags,
			1, &*command_buffers[image_index],
			1, &*render_finished[frame_index]
		};

		graphics_queue.submit({submit_info}, *in_flight_fences[frame_index]);
		std::cout << "Submitting command buffer...\n";

		/* Wait for the fence to be signaled
		result = device.waitForFences(
			*fences[image_index],
			true,
			std::numeric_limits <uint64_t> ::max()
		); */

		KOBRA_ASSERT(result == vk::Result::eSuccess, "Failed to wait for fence");

		present_swapchain_image(swapchain, image_index, present_queue, render_finished[frame_index]);
		std::cout << "Presenting image...\n";

		// Poll events
		glfwPollEvents();

		input_handling(window.handle);
		std::cout << "\nin_flight_fence[" << frame_index << "] = " << *in_flight_fences[frame_index] << "\n";
		std::cout << "images_in_flight[" << image_index << "] = " << *images_in_flight[image_index] << "\n";

		in_flight_fences[frame_index] = std::move(images_in_flight[image_index]);

		// Update frame index
		frame_index = (frame_index + 1) % 2;
		std::cout << "==============================\n\n";
		break;
	}

	// Wait before exiting
	device.waitIdle();
}