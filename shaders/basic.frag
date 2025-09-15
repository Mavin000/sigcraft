#version 450
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0)
in vec3 color;

layout(location = 1)
in vec3 normal;

layout(location = 2)
in vec2 uv;

layout(location = 3)
flat in uint texId;

layout(location = 0)
out vec4 colorOut;

layout(set = 0, binding = 0)
uniform sampler nn;

layout(set = 0, binding = 1)
uniform texture2D textures[14];

void main() {
    colorOut = vec4(normal * 0.5 + vec3(0.5), 1.0);
    colorOut = vec4(color * 0.8 + 0.2 * dot(normal, normalize(vec3(1.0, 0.5, 0.1))), 1.0);
    colorOut = texture(sampler2D(textures[nonuniformEXT(texId)], nn), uv);
}