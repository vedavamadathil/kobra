#ifndef UI_ELEMENT_H_
#define UI_ELEMENT_H_

// GLM
#include <glm/glm.hpp>

namespace mercury {

namespace ui {

class UIElement {
public:
	virtual void draw() = 0;

	virtual glm::vec2 get_position() const = 0;
	virtual void set_position(const glm::vec2 &) = 0;

	virtual void move(const glm::vec2 &dpos) {
		glm::vec2 pos = get_position();
		set_position(pos + dpos);
	}
};

}

}

#endif
