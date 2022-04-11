// Standard headers
#include <cstdio>
#include <optional>

// Engine headers
#include "../include/mesh.hpp"
#include "../include/model.hpp"
#include "../include/profiler.hpp"
#include "../include/scene.hpp"
#include "../include/sphere.hpp"

namespace kobra {

//////////////////////
// Helper functions //
//////////////////////

// TODO: loading models (with appropriate materials, etc)
static ObjectPtr load_object(const Vulkan::Context &ctx,
		const VkCommandPool &command_pool,
		std::ifstream &fin, const std::string &path)
{
	std::string header;

	// Read object intro header
	std::getline(fin, header);
	if (header != "[OBJECT]") {
		KOBRA_LOG_FUNC(error) << "Expected [OBJECT] header, got "
			<< header << std::endl;
		return nullptr;
	}

	// Get object name
	std::string name;
	std::getline(fin, header);

	// Get substring (name=...)
	auto pos = header.find('=');
	if (pos == std::string::npos) {
		KOBRA_LOG_FUNC(error) << "Expected '=' in object name, got "
			<< header << std::endl;
		return nullptr;
	}

	name = header.substr(pos + 1);

	// Read transform header
	std::getline(fin, header);
	if (header != "[TRANSFORM]") {
		KOBRA_LOG_FUNC(error) << "Expected [TRANSFORM] header, got "
			<< header << std::endl;
		return nullptr;
	}

	auto t = Transform::from_file(fin);
	if (!t)
		return nullptr;
	std::cout << "Loaded transform:\n";
	glm::vec3 v = t->position;
	std::cout << "\tPosition: " << v.x << ", " << v.y << ", " << v.z << std::endl;

	// Read the object header
	std::getline(fin, header);

	// Switch on the object type
	if (header == "[SPHERE]") {
		Profiler::one().frame("Loading sphere");
		auto sphere = Sphere::from_file(ctx, command_pool, fin, path);
		Profiler::one().end();

		if (!sphere)
			return nullptr;

		auto optr = ObjectPtr(new Sphere(*sphere, *t));
		optr->set_name(name);
		optr->transform() = *t;
		return optr;
	}

	if (header == "[MESH]") {
		Profiler::one().frame("Loading mesh");
		auto mesh = Mesh::from_file(ctx, command_pool, fin, path);
		Profiler::one().end();

		if (!mesh)
			return nullptr;

		auto optr = ObjectPtr(new Mesh(*mesh, *t));
		optr->set_name(name);
		optr->transform() = *t;
		return optr;
	}

	// Else
	KOBRA_LOG_FUNC(error) << "Unknown object type: \"" << header << "\"\n";
	return nullptr;
}

//////////////////
// Constructors //
//////////////////

Scene::Scene(const Vulkan::Context &ctx,
		const VkCommandPool &command_pool,
		const std::string &filename)
{
	// Open the file
	std::ifstream fin(filename);

	// Check if the file is open
	if (!fin.is_open()) {
		KOBRA_LOG_FUNC(error) << "Could not open file " << filename << "\n";
		return;
	}

	// Load all objects
	while (!fin.eof()) {
		// Break if the rest of the file is empty
		if (fin.peek() == EOF)
			break;

		// Read the next object
		Profiler::one().frame("Loading object");
		ObjectPtr obj = load_object(ctx, command_pool, fin, filename);
		Profiler::one().end();

		// Check if the object is valid
		if (obj) {
			// Add the object to the scene
			KOBRA_LOG_FUNC(notify) << "Loaded object \"" << obj->name() << "\"\n";
			_objects.push_back(obj);
		} else {
			// Skip the line
			std::string line;
			std::getline(fin, line);

			KOBRA_LOG_FUNC(warn) << "Skipping invalid object\n";
			break;
		}
	}
}

Scene::Scene(const std::vector <Object *> &objs)
{
	for (auto &obj : objs)
		_objects.push_back(ObjectPtr(obj));
}

Scene::Scene(const std::vector <ObjectPtr> &objs)
		: _objects(objs) {}

/////////////
// Methods //
/////////////

// Iterators
Scene::iterator Scene::begin() const
{
	return _objects.begin();
}

Scene::iterator Scene::end() const
{
	return _objects.end();
}

// Indexing
ObjectPtr Scene::operator[](const std::string &name) const
{
	for (auto &obj : _objects) {
		if (obj->name() == name)
			return obj;
	}

	return nullptr;
}

// Saving
void Scene::save(const std::string &filename) const
{
	// Open the file
	std::ofstream file(filename);

	// Check if the file is open
	if (!file.is_open()) {
		KOBRA_LOG_FUNC(error) << "Could not open file " << filename << "\n";
		return;
	}

	// Write each object
	for (auto &obj : _objects)
		obj->save_object(file);
}

}
