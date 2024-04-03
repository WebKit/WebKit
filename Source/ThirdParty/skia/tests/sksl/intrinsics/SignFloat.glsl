
out vec4 sk_FragColor;
uniform vec4 testInputs;
uniform vec4 colorGreen;
uniform vec4 colorRed;
vec4 main() {
    vec4 expected = vec4(-1.0, 0.0, 1.0, 1.0);
    return ((((((sign(testInputs.x) == expected.x && sign(testInputs.xy) == expected.xy) && sign(testInputs.xyz) == expected.xyz) && sign(testInputs) == expected) && -1.0 == expected.x) && vec2(-1.0, 0.0) == expected.xy) && vec3(-1.0, 0.0, 1.0) == expected.xyz) && vec4(-1.0, 0.0, 1.0, 1.0) == expected ? colorGreen : colorRed;
}
