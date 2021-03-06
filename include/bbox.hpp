#ifndef BOUNDING_BOX_H_
#define BOUNDING_BOX_H_

// GLM headers
#include <glm/glm.hpp>

namespace kobra {

// Axis Aligned Bounding Box
struct BoundingBox {
	glm::vec3	min;
	glm::vec3	max;
	int		id = -1;
	
	// Get surface area of box
	float surface_area() const {
		float dx = max.x - min.x;
		float dy = max.y - min.y;
		float dz = max.z - min.z;

		float xy = dx * dy;
		float yz = dy * dz;
		float xz = dx * dz;

		return 2.0f * (xy + yz + xz);
	}
};

}

#endif
