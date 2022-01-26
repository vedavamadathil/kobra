#ifndef CAMERA_H_
#define CAMERA_H_

// GLM headers
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Engine headers
#include "ray.hpp"
#include "transform.hpp"

// Camera properties
class Tunings {
        float _radians(float degrees) {
                return degrees * (M_PI / 180.0f);
        }
public:
        float fov;
        float scale;
        float aspect;

        // TODO: add lens attributes
        Tunings() : fov(90.0f), aspect(1.0f) {
                scale = tan(_radians(fov) * 0.5f);
        }

        Tunings(float fov, float width, float height)
                        : fov(fov), aspect(width / height) {
                scale = tan(_radians(fov) * 0.5f);
        }
};

// Camera class
class Camera {
        // Camera properties
        Tunings tunings;
public:
        // Public member variables
        Transform transform;

        // Constructors
        Camera();
        Camera(const Transform& trans, const Tunings& tns)
                        : transform(trans), tunings(tns) {}

        // Ray generation
        Ray ray(float nx, float ny) const {
                float cx = (nx * 2.0f - 1.0f) * tunings.scale * tunings.aspect;
                float cy = (1.0f - ny * 2.0f) * tunings.scale;

                glm::vec3 dir = cx * transform.right
                        + cy * transform.up
                        + transform.forward;

                return Ray {
                        transform.position,
                        glm::normalize(dir)
                };
        }
};

#endif