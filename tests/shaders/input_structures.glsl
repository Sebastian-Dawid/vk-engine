layout (set = 0, binding = 0) uniform scene_data_t
{
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec4 ambient_color;
    vec4 sunlight_dir;
    vec4 sunlight_color;
} scene_data;

layout (set = 1, binding = 0) uniform gltf_material_data_t
{
    vec4 color_factors;
    vec4 metal_rough_factors;
} material_data;

layout (set = 1, binding = 1) uniform sampler2D color_tex;
layout (set = 1, binding = 2) uniform sampler2D meatl_rough_tex;
