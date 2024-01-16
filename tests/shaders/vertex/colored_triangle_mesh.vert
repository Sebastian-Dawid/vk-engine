#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 out_color;
layout (location = 1) out vec2 out_uv;

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
    mat4 world;
    vertex_buffer_t vertex_buffer;
} push_constants;

void main()
{
    vertex_t v = push_constants.vertex_buffer.vertices[gl_VertexIndex];
    gl_Position = push_constants.world * vec4(v.position, 1);
    out_color = v.color.xyz;
    out_uv = v.uv;
}
