#ifndef COORDS_H_
#define COORDS_H_

// GLM headers
#include <glm/glm.hpp>

namespace mercury {

namespace coordinates {

// Screen structure
struct Screen {
	float x;
	float y;

	size_t width;
	size_t height;

	// Convert Screen to NDC
	glm::vec2 to_ndc() {
		return {
			(2.0f * x) / width - 1.0f,
			1.0f - (2.0f * y) / height
		};
	}
};

}

}

#endif