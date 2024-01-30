#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "../input_structures.glsl"

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec3 out_color;
layout (location = 2) out vec2 out_uv;

layout (location = 0) in mat4 in_transform;

struct vertex_t
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 color;
};

layout (buffer_reference, std430) readonly buffer vertex_buffer_t
{
    vertex_t vertices[];
};

layout (push_constant) uniform constants
{
    mat4 render_matrix;
    vertex_buffer_t vertex_buffer;
} push_constants;

void main()
{

}