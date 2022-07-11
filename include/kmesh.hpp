#ifndef KOBRA_KMESH_H_
#define KOBRA_KMESH_H_

// Standard headers
#include <optional>

// Engine headers
#include "logger.hpp"	// TODO: remove
#include "transform.hpp"
#include "types.hpp"
#include "vertex.hpp"
#include "object.hpp"
#include "renderable.hpp"

namespace kobra {

// KMesh class, just holds a list of vertices and indices
class KMesh : virtual public Object, virtual public Renderable {
public:
	static constexpr char object_type[] = "KMesh";
protected:
	// Potential source
	std::string	_source;
	int		_source_index = -1;

	// Vertices and indices
	VertexList 	_vertices;
	Indices		_indices;

	// Process tangent and bitangent
	void _process_vertex_data() {
		// Iterate over all vertices
		for (int i = 0; i < _vertices.size(); i++) {
			// Get the vertex
			Vertex &v = _vertices[i];

			// Calculate tangent and bitangent
			v.tangent = glm::vec3(0.0f);
			v.bitangent = glm::vec3(0.0f);

			// Iterate over all faces
			for (int j = 0; j < _indices.size(); j += 3) {
				// Get the face
				int face0 = _indices[j];
				int face1 = _indices[j + 1];
				int face2 = _indices[j + 2];

				// Check if the vertex is part of the face
				if (face0 == i || face1 == i || face2 == i) {
					// Get the face vertices
					const auto &v1 = _vertices[face0];
					const auto &v2 = _vertices[face1];
					const auto &v3 = _vertices[face2];

					glm::vec3 e1 = v2.position - v1.position;
					glm::vec3 e2 = v3.position - v1.position;

					glm::vec2 uv1 = v2.tex_coords - v1.tex_coords;
					glm::vec2 uv2 = v3.tex_coords- v1.tex_coords;

					float r = 1.0f / (uv1.x * uv2.y - uv1.y * uv2.x);
					glm::vec3 tangent = (e1 * uv2.y - e2 * uv1.y) * r;
					glm::vec3 bitangent = (e2 * uv1.x - e1 * uv2.x) * r;

					// Add the tangent and bitangent to the vertex
					v.tangent += tangent;
					v.bitangent += bitangent;
				}
			}

			// Normalize the tangent and bitangent
			v.tangent = glm::normalize(v.tangent);
			v.bitangent = glm::normalize(v.bitangent);
		}
	}
public:
	// Default constructor
	KMesh() = default;

	// Copy constructor
	KMesh(const KMesh &mesh)
			: Object(object_type, mesh.transform()),
			Renderable(mesh.material),
			_source(mesh._source),
			_source_index(mesh._source_index),
			_vertices(mesh._vertices),
			_indices(mesh._indices) {}

	// Copy assignment operator
	KMesh &operator=(const KMesh &mesh) {
		// Check for self assignment
		if (this != &mesh) {
			// Copy the object
			Object::operator=(mesh);
			Renderable::operator=(mesh.material);
			_source = mesh._source;
			_source_index = mesh._source_index;
			_vertices = mesh._vertices;
			_indices = mesh._indices;
		}

		return *this;
	}

	// Simple constructor
	// TODO: load tangent and bitangent in loading models
	KMesh(const KMesh &mesh, const Transform &transform)
			: Object(object_type, transform),
			Renderable(mesh.material),
			_source(mesh._source),
			_source_index(mesh._source_index),
			_vertices(mesh._vertices),
			_indices(mesh._indices) {}

	KMesh(const VertexList &vs, const Indices &is,
			const Transform &t = Transform(),
			bool calculate_tangents = true)
			: Object(object_type, t),
			_vertices(vs), _indices(is) {
		// Process the vertex data
		if (calculate_tangents)
			_process_vertex_data();
	}

	// Copy vertex and index data from another mesh
	void copy_data(const KMesh &mesh) {
		_vertices = mesh._vertices;
		_indices = mesh._indices;
	}

	// Properties
	size_t vertex_count() const {
		return _vertices.size();
	}

	size_t triangle_count() const {
		return _indices.size() / 3;
	}

	// Centroid of the mesh
	glm::vec3 center() const override {
		glm::vec3 s {0.0f, 0.0f, 0.0f};

		// Sum the centroids of all triangles
		for (size_t i = 0; i < _indices.size(); i += 3) {
			s += _vertices[_indices[i]].position;
			s += _vertices[_indices[i + 1]].position;
			s += _vertices[_indices[i + 2]].position;
		}

		// Divide by the number of triangles
		return _transform.apply(s / (3.0f * triangle_count()));
	}

	// Get data
	const VertexList &vertices() const {
		return _vertices;
	}

	const Indices &indices() const {
		return _indices;
	}

	// Ray intersection
	float intersect(const Ray &ray) const override {
		// Shortest time to intersection so far
		float t = std::numeric_limits <float> ::max();

		// Lambda helper to calculate the time
		//	to intersection for a given triangle
		auto time_to_triangle = [&ray](const glm::vec3 &a,
				const glm::vec3 b,
				const glm::vec3 &c) -> float {
			glm::vec3 e1 = b - a;
			glm::vec3 e2 = c - a;

			glm::vec3 p = glm::cross(ray.direction, e2);
			float det = glm::dot(e1, p);
			if (det > -0.00001f && det < 0.00001f)
				return -1;

			float inv_det = 1.0f / det;
			glm::vec3 tvec = ray.origin - a;
			float u = glm::dot(tvec, p) * inv_det;
			if (u < 0.0f || u > 1.0f)
				return -1;

			glm::vec3 q = glm::cross(tvec, e1);
			float v = glm::dot(ray.direction, q) * inv_det;
			if (v < 0.0f || u + v > 1.0f)
				return -1;

			return glm::dot(e2, q) * inv_det;
		};

		// Iterate over all triangles
		for (size_t i = 0; i < _indices.size(); i += 3) {
			// Get the triangle vertices
			const auto &v1 = _vertices[_indices[i]];
			const auto &v2 = _vertices[_indices[i + 1]];
			const auto &v3 = _vertices[_indices[i + 2]];

			auto tv1 = _transform.apply(v1.position);
			auto tv2 = _transform.apply(v2.position);
			auto tv3 = _transform.apply(v3.position);

			// Calculate the time to intersection
			float t_triangle = time_to_triangle(tv1, tv2, tv3);

			// Check if the ray intersects the triangle
			if (t_triangle > 0.0f && t_triangle < t)
				t = t_triangle;
		}

		// Return the time to intersection
		return t;
	}

	// Virtual methods
	void save(std::ofstream &) const override;

	// KMesh factories
	// TODO: should formaat with struct arguments and overloads instead:
	// KMesh make(Box{...});
	// KMesh make(Sphere{...}); ...
	static KMesh make_box(const glm::vec3 &, const glm::vec3 &);
	static KMesh make_sphere(const glm::vec3 &, float, int = 16, int = 16);
	static KMesh make_ring(const glm::vec3 &, float, float, float, int = 32);

	// Read from file
	static std::optional <KMesh> from_file(vk::raii::PhysicalDevice &,
		vk::raii::Device &,
		vk::raii::CommandPool &,
		std::ifstream &,
		const std::string &);

	// Friends
	friend class Model;
};

// Get closest distance between ray and mesh
// TODO: move out of here
struct cd_value {
	float		distance;
	glm::vec3	rayp;
	glm::vec3	objp;
};

inline cd_value closest_distance(const KMesh &mesh, const Ray &ray)
{
	const VertexList &vs = mesh.vertices();

	float min = std::numeric_limits <float> ::max();

	glm::vec3 rp;
	glm::vec3 op;

	for (auto &v : vs) {
		glm::vec3 p = mesh.transform().apply(v.position);

		float d = glm::length(glm::cross(ray.direction, p - ray.origin));
		if (d < min) {
			min = d;
			rp = ray.origin + ray.direction * glm::dot(ray.direction, p - ray.origin);
			op = p;
		}
	}

	return {min, rp, op};
}

}

#endif