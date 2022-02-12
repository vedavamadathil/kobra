#ifndef MESH_H_
#define MESH_H_

// Engine headers
#include "primitive.hpp"

// TODO: put vertex into own header file

// Vertex types
enum VertexType : uint32_t {
	VERTEX_TYPE_POSITION	= 0,
	VERTEX_TYPE_NORMAL	= 0x1,
	VERTEX_TYPE_TEXCOORD	= 0x2,
	VERTEX_TYPE_COLOR	= 0x4,
	VERTEX_TYPE_TANGENT	= 0x8
};

// Position vertex
struct PVert {
	glm::vec3 pos;
};

// Vertex information (templated by vertex type)
template <VertexType T = VERTEX_TYPE_POSITION>
struct Vertex : public PVert {
	// Vertex type
	static constexpr VertexType type = T;

	// Vertex constructor
	Vertex() {}

	// Vertex constructor
	Vertex(const glm::vec3& pos) : PVert {pos} {}
};

// TODO: vertex type specializations

// Mesh class
template <VertexType T = VERTEX_TYPE_POSITION>
class Mesh : public Primitive {
public:
	// Public aliases
	using VertexList = std::vector <Vertex<T>>;
private:
	// List of vertices
	std::vector <Vertex <T>>	_vertices;

	// List of indices
	Indices				_indices;
public:
	// Constructors
	Mesh() {}
	Mesh(const VertexList &vertices, const Indices &indices,
			const Material &material)
			: _vertices {vertices}, _indices {indices},
			Primitive {OBJECT_TYPE_NONE, Transform(), material} {}

	// Write mesh to buffer (fake to resolve abstract base class)
	void write(Buffer &buffer) const override {
		// Throw
		throw std::runtime_error("Mesh::write() not implemented");
	}

	// Write mesh to buffer
	// TODO: write to both vertex and object buffers
	void write_to_buffer(Buffer &buffer, Indices &indices, uint mati) override {
		// Remove last index
		indices.erase(indices.end() - 1);

		// Write each triangle
		for (size_t i = 0; i < _indices.size(); i += 3) {
			// Get each vertex
			const Vertex <T> &v0 = _vertices[_indices[i + 0]];
			const Vertex <T> &v1 = _vertices[_indices[i + 1]];
			const Vertex <T> &v2 = _vertices[_indices[i + 2]];

			// Construct triangle
			Triangle triangle {
				v0.pos, v1.pos, v2.pos,
				material
			};

			// Write triangle to buffer
			indices.push_back(buffer.size());
			triangle.write_to_buffer(buffer, indices, mati);
		}
	}
};

#endif