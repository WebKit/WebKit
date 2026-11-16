
out vec4 sk_FragColor;
layout (binding = 0) uniform sampler2D t;
void main() {
    uvec2 dims = textureSize(t);
    sk_FragColor = texture(t, vec2(dims));
}
