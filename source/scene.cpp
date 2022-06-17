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
static ObjectPtr load_object(vk::raii::PhysicalDevice &phdev,
		vk::raii::Device &device,
		vk::raii::CommandPool &command_pool,
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

	// Read the object header
	std::getline(fin, header);

	// Switch on the object type
	if (header == "[SPHERE]") {
		KOBRA_LOG_FUNC(notify) << "Loading sphere " << name << std::endl;
		Profiler::one().frame("Loading sphere");
		auto sphere = Sphere::from_file(phdev, device, command_pool, fin, path);
		Profiler::one().end();

		if (!sphere)
			return nullptr;

		auto optr = ObjectPtr(new Sphere(*sphere, *t));
		optr->set_name(name);
		optr->transform() = *t;
		return optr;
	}

	if (header == "[MESH]") {
		KOBRA_LOG_FUNC(notify) << "Loading mesh " << name << std::endl;
		Profiler::one().frame("Loading mesh");
		auto mesh = Mesh::from_file(phdev, device, command_pool, fin, path);
		Profiler::one().end();

		if (!mesh)
			return nullptr;

		KOBRA_LOG_FUNC(notify) << "Finshed loading model, creating object" << std::endl;
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

Scene::Scene(vk::raii::PhysicalDevice &phdev,
		vk::raii::Device &device,
		vk::raii::CommandPool &command_pool,
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
		ObjectPtr obj = load_object(
			phdev, device, command_pool,
			fin, filename
		);

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

// Add object
void Scene::add(const ObjectPtr &obj)
{
	_objects.push_back(obj);
}

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

// Erase object
void Scene::erase(const std::string &name)
{
	for (auto it = _objects.begin(); it != _objects.end(); ++it) {
		if ((*it)->name() == name) {
			_objects.erase(it);
			return;
		}
	}
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
