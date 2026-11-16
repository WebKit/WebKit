"use strict";

let subcases = [];
for (let contextType of [ "webgl", "webgl2" ]) {
    subcases.push({ contextType });
}

function runTest() {
    for (const subcase of subcases) {
        const canvas = document.createElement("canvas");
        const gl = canvas.getContext(subcase.contextType);
        if (!gl) {
            testPassed(`${subcase.contextType}: Not supported.`);
            continue;
        }
        const shader = gl.createShader(gl.FRAGMENT_SHADER);
        gl.shaderSource(shader, "void main() { gl_FragColor = vec4(gl_FragCoord.x, 0.0, 0.0, 1.0); }");
        gl.compileShader(shader);
        const src = gl.getExtension("WEBGL_debug_shaders").getTranslatedShaderSource(shader);
        let type = "Unknown";
        if (src.search(/^fragment ANGLE_FragmentOut main/m) >= 0)
            type = "Metal";
        if (src.search(/^void main()/m) >= 0)
            type = "OpenGL";
        testPassed(`${subcase.contextType}: Detected shader implementation language: ${type}`);
    }
}