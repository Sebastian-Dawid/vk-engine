#pragma once

#include <vk-types.h>
#include <GLFW/glfw3.h>

struct camera_t
{
    glm::vec3 velocity;
    glm::vec3 position;
    float pitch {0.f};
    float yaw {0.f};

    glm::mat4 get_view_matrix();
    glm::mat4 get_rotation_matrix();

    void process_glfw_event(GLFWwindow* window);

    void update();
};

void cursor_pos_callback(GLFWwindow* window, double x_pos, double y_pos);
