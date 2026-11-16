function webglDrawImage(canvas, image) {

    var gl = canvas.getContext("webgl");

    // Set the clear color.
    gl.clearColor(0, 0, 0, 1);

    // Create the vertex shader object.
    var vertexShader = gl.createShader(gl.VERTEX_SHADER);

    // The source code for the shader is extracted from the <script> element above.
    gl.shaderSource(vertexShader, document.getElementById("vertexShaderSource").textContent);

    // Compile the shader.
    gl.compileShader(vertexShader);
    if (!gl.getShaderParameter(vertexShader, gl.COMPILE_STATUS)) {
        // We failed to compile. Output to the console and quit.
        console.error("Vertex Shader failed to compile.");
        console.log(gl.getShaderInfoLog(vertexShader));
        return;
    }

    // Do the fragment shader.
    var fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fragmentShader, document.getElementById("fragmentShaderSource").textContent);
    gl.compileShader(fragmentShader);
    if (!gl.getShaderParameter(fragmentShader, gl.COMPILE_STATUS)) {
        console.error("Fragment Shader failed to compile.");
        console.log(gl.getShaderInfoLog(fragmentShader));
        return;
    }

    // Make the program.
    var program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);

    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
        console.error("Unable to link shaders into program.");
        return;
    }

    gl.useProgram(program);

    var textureUniform = gl.getUniformLocation(program, "texture");

    var positionAttribute = gl.getAttribLocation(program, "a_position");
    gl.enableVertexAttribArray(positionAttribute);

    // The default coordinate space is a square from -1 to 1.
    // The triangle is three points in that square.
    var vertices = new Float32Array([
        -1, -1,
         1, -1,
         1,  1,
         1,  1,
        -1,  1,
        -1, -1
    ]);

    // Create the buffer that will hold the vertex data above.
    var triangleBuffer = gl.createBuffer();

    // Load the vertex data into the buffer.
    gl.bindBuffer(gl.ARRAY_BUFFER, triangleBuffer);
    gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);

    var stillToDraw = false;

    var updateTexture = function (texture, data) {
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, data);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

        gl.bindTexture(gl.TEXTURE_2D, null);
        stillToDraw = true;
    }

    var texture = gl.createTexture();
    updateTexture(texture, image);

    // Clear to black.
    gl.clear(gl.COLOR_BUFFER_BIT);

    var drawFunction = function () {
        if (!stillToDraw) {
            window.requestAnimationFrame(drawFunction);
        }

        // Clear to black.
        gl.clear(gl.COLOR_BUFFER_BIT);

        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.uniform1i(textureUniform, 0);

        // Bind the vertex attributes for the draw operation.
        gl.bindBuffer(gl.ARRAY_BUFFER, triangleBuffer);
        gl.vertexAttribPointer(positionAttribute, 2, gl.FLOAT, false, 0, 0);

        gl.drawArrays(gl.TRIANGLES, 0, 6);
    };

    drawFunction();
}
