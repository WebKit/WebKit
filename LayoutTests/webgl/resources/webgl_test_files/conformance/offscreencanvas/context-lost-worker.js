/*
Copyright (c) 2019 The Khronos Group Inc.
Use of this source code is governed by an MIT-style license that can be
found in the LICENSE.txt file.
*/

importScripts("../../js/tests/canvas-tests-utils.js");
self.onmessage = function(e) {
    canvas = new OffscreenCanvas(10, 10);
    gl = canvas.getContext('webgl');

    // call testValidContext() before checking for the extension, because this is where we check
    // for the isContextLost() method, which we want to do regardless of the extension's presence.
    if (!testValidContext())
        self.postMessage("Test failed");

    extension = gl.getExtension("WEBGL_lose_context");
    // need an extension that exposes new API methods.
    OES_vertex_array_object = gl.getExtension("OES_vertex_array_object");
    if (extension == null || OES_vertex_array_object == null)
        self.postMessage("Test failed");

    // We need to initialize |uniformLocation| before losing context.
    // Otherwise gl.getUniform() when context is lost will throw.
    uniformLocation = gl.getUniformLocation(program, "tex");
    extension.loseContext();

    canvas.addEventListener("webglcontextlost", function() {
        if (testLostContextWithoutRestore())
            self.postMessage("Test passed");
        else
            self.postMessage("Test failed");
    }, false);
}

