function generateTest(pixelFormat, pixelType, prologue, errorRange) {
    errorRange = errorRange || 0;
    var wtu = WebGLTestUtils;
    var gl = null;
    var textureLoc = null;
    var successfullyParsed = false;

    var init = function()
    {
        description('Verify texImage2D and texSubImage2D code paths taking video elements (' + pixelFormat + '/' + pixelType + ')');

        gl = wtu.create3DContext("example");

        if (!prologue(gl)) {
            finishTest();
            return;
        }

        var program = wtu.setupTexturedQuad(gl);

        gl.clearColor(0,0,0,1);
        gl.clearDepth(1);

        textureLoc = gl.getUniformLocation(program, "tex");

        var video = document.getElementById("vid");
        video.addEventListener(
            "playing", function() { runTest(video); }, true);
        video.loop = true;
        video.play();
    }

    function runOneIteration(videoElement, useTexSubImage2D, flipY, topColorCheck, bottomColorCheck)
    {
        debug('Testing ' + (useTexSubImage2D ? 'texSubImage2D' : 'texImage2D') +
              ' with flipY=' + flipY);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        // Disable any writes to the alpha channel
        gl.colorMask(1, 1, 1, 0);
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
        // Upload the videoElement into the texture
        if (useTexSubImage2D) {
            // Initialize the texture to black first
            gl.texImage2D(gl.TEXTURE_2D, 0, gl[pixelFormat],
                          videoElement.videoWidth, videoElement.videoHeight, 0,
                          gl[pixelFormat], gl[pixelType], null);
            gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, gl[pixelFormat], gl[pixelType], videoElement);
        } else {
            gl.texImage2D(gl.TEXTURE_2D, 0, gl[pixelFormat], gl[pixelFormat], gl[pixelType], videoElement);
        }

        var c = document.createElement("canvas");
        c.width = 16;
        c.height = 16;
        c.style.border = "1px solid black";
        var ctx = c.getContext("2d");
        ctx.drawImage(videoElement, 0, 0, c.width, c.height);
        document.body.appendChild(c);
        var frame = ctx.getImageData(0, 0, c.width, c.height);
        var getPixel = (x, y) => { const offset = (y * frame.width + x) * 4; return [ frame.data[offset], frame.data[offset + 1], frame.data[offset + 2] ]; };
        var topColor = getPixel(4, flipY ? 4 : (c.height - 4));
        if (!topColorCheck(topColor)) {
            testFailed("Unexpected 2d top color " + topColor);
        }
        var bottomColor = getPixel(4, flipY ? (c.height - 4) : 4);
        if (!bottomColorCheck(bottomColor)) {
            testFailed("Unexpected 2d bottom color " + bottomColor);
        }

        // Point the uniform sampler to texture unit 0
        gl.uniform1i(textureLoc, 0);
        // Draw the triangles
        wtu.drawQuad(gl, [0, 0, 0, 255]);
        // Check a few pixels near the top and bottom and make sure they have
        // the right color.
        debug("Checking lower left corner");
        wtu.checkCanvasRect(gl, 4, 4, 2, 2, bottomColor,
                            "shouldBe " + bottomColor, errorRange);
        debug("Checking upper left corner");
        wtu.checkCanvasRect(gl, 4, gl.canvas.height - 8, 2, 2, topColor,
                            "shouldBe " + topColor, errorRange);
    }

    function runTest(videoElement)
    {
        var redish = color => color[0] > 192 && color[1] < 64 && color[2] < 64;
        var greenish = color => color[0] < 64 && color[1] > 192 && color[2] < 64;
        runOneIteration(videoElement, false, true, redish, greenish);
        runOneIteration(videoElement, false, false, greenish, redish);
        runOneIteration(videoElement, true, true, redish, greenish);
        runOneIteration(videoElement, true, false, greenish, redish);

        glErrorShouldBe(gl, gl.NO_ERROR, "should be no errors");
        finishTest();
    }

    return init;
}
