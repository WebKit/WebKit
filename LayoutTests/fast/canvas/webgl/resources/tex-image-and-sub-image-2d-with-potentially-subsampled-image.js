function generateTest(pixelFormat, pixelType, pathToTestRoot, prologue) {
    var wtu = WebGLTestUtils;
    var gl = null;
    var textureLoc = null;
    var successfullyParsed = false;
    var imgCanvas;
    var red = [255, 0, 0];
    var green = [0, 255, 0];
    var blue = [0, 0, 255];

    var init = function()
    {
        if (window.initNonKhronosFramework) {
            window.initNonKhronosFramework(true);
        }

        description('Verify texImage2D and texSubImage2D code paths with potentially subsampled images (' + pixelFormat + '/' + pixelType + ')');

        gl = wtu.create3DContext("example");

        if (!prologue(gl)) {
            finishTest();
            return;
        }

        var program = wtu.setupTexturedQuad(gl);

        gl.clearColor(0,0,0,1);
        gl.clearDepth(1);

        textureLoc = gl.getUniformLocation(program, "tex");

        wtu.loadTexture(gl, pathToTestRoot + "/../resources/image-6400x6400.jpg", runTest);
    }

    function runOneIteration(image, useTexSubImage2D, flipY, topColor, middleColor, bottomColor)
    {
        debug('Testing ' + (useTexSubImage2D ? 'texSubImage2D' : 'texImage2D') +
              ' with flipY=' + flipY);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        var texture = gl.createTexture();
        // Bind the texture to texture unit 0
        gl.bindTexture(gl.TEXTURE_2D, texture);
        // Set up texture parameters
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        // Set up pixel store parameters
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, flipY);
        gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
        gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, gl.NONE);
        // Upload the image into the texture
        if (useTexSubImage2D) {
            // Initialize the texture to black first
            gl.texImage2D(gl.TEXTURE_2D, 0, gl[pixelFormat], image.width, image.height, 0,
                          gl[pixelFormat], gl[pixelType], null);
            gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, gl[pixelFormat], gl[pixelType], image);
        } else {
            gl.texImage2D(gl.TEXTURE_2D, 0, gl[pixelFormat], gl[pixelFormat], gl[pixelType], image);
        }

        // Point the uniform sampler to texture unit 0
        gl.uniform1i(textureLoc, 0);
        // Draw the triangles
        wtu.drawQuad(gl, [0, 0, 0, 255]);

        // Check the pixels at the top left, middle and bottom right.

        debug("Checking upper left corner");
        wtu.checkCanvasRect(gl, 2, gl.canvas.height - 2, 1, 1, topColor, "shouldBe " + topColor, 5);
        debug("Checking middle");
        wtu.checkCanvasRect(gl, gl.canvas.width / 2, gl.canvas.height / 2, 1, 1, middleColor, "shouldBe " + middleColor, 5);
        debug("Checking bottom right corner");
        wtu.checkCanvasRect(gl, gl.canvas.width - 2, 2, 1, 1, bottomColor, "shouldBe " + bottomColor, 5);
    }

    function runTestOnImage(image) {
        runOneIteration(image, false, true, red, green, blue);
        runOneIteration(image, false, false, green, green, green);
        runOneIteration(image, true, true, red, green, blue);
        runOneIteration(image, true, false, green, green, green);
    }

    function runTest(image)
    {
        runTestOnImage(image);
        glErrorShouldBe(gl, gl.NO_ERROR, "should be no errors");
        finishTest();
    }

    return init;
}
