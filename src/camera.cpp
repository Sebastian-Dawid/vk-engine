#include <camera.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

static float last_frame = 0.f;

void camera_t::update()
{
    float current_time = glfwGetTime();
    float delta_time = current_time - last_frame;
    last_frame = current_time;
    glm::mat4 camera_rotation = this->get_rotation_matrix();
    this->position += glm::vec3(camera_rotation * glm::vec4(this->velocity * delta_time, 0.f));
}

void camera_t::process_glfw_event(GLFWwindow* window)
{
    this->velocity = glm::vec3(0);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) this->velocity.z = -1;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) this->velocity.x = -1;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) this->velocity.z =  1;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) this->velocity.x =  1;
}

glm::mat4 camera_t::get_view_matrix()
{
    glm::mat4 camera_translation = glm::translate(glm::mat4(1.f), this->position);
    glm::mat4 camera_rotation = this->get_rotation_matrix();
    return glm::inverse(camera_translation * camera_rotation);
}

glm::mat4 camera_t::get_rotation_matrix()
{
    glm::quat pitch_rotation = glm::angleAxis(this->pitch, glm::vec3(1.f, 0.f, 0.f));
    glm::quat yaw_rotation = glm::angleAxis(this->yaw, glm::vec3(0.f, -1.f, 0.f));
    return glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);
}

static double last_x = 0, last_y = 0;
static bool first_move = true;

void cursor_pos_callback(GLFWwindow *window, double x_pos, double y_pos)
{
    if (first_move)
    {
        last_x = x_pos;
        last_y = y_pos;
        first_move = false;
    }
    camera_t* cam = (camera_t*)glfwGetWindowUserPointer(window);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        cam->yaw += (x_pos - last_x) / 400.f;
        cam->pitch -= (y_pos - last_y) / 400.f;
    }
    last_x = x_pos;
    last_y = y_pos;
}
