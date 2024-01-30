#version 450

#extension GL_GOOGLE_include_directive : require
#include "../input_structures.glsl"

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_uv;

layout (location = 0) out vec3 out_pos;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec4 out_color;

void main()
{
    out_pos = in_pos;
    out_color.rgb = texture(color_tex, in_uv).rgb;
    out_color.a = texture(metal_rough_tex, in_uv).r;
    out_normal = normalize(in_normal);
}
