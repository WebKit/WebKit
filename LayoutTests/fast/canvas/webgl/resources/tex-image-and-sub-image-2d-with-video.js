// This block needs to be outside the onload handler in order for this
// test to run reliably in WebKit's test harness (at least the
// Chromium port). https://bugs.webkit.org/show_bug.cgi?id=87448
if (window.initNonKhronosFramework) {
    window.initNonKhronosFramework(true);
}

function generateTest(pixelFormat, pixelType, prologue) {
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

    function runOneIteration(videoElement, useTexSubImage2D, flipY, topColor, bottomColor)
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
        ctx.drawImage(videoElement, 0, 0, 16, 16);
        document.body.appendChild(c);

        // Point the uniform sampler to texture unit 0
        gl.uniform1i(textureLoc, 0);
        // Draw the triangles
        wtu.drawQuad(gl, [0, 0, 0, 255]);
        // Check a few pixels near the top and bottom and make sure they have
        // the right color.
        debug("Checking lower left corner");
        wtu.checkCanvasRect(gl, 4, 4, 2, 2, bottomColor,
                            "shouldBe " + bottomColor);
        debug("Checking upper left corner");
        wtu.checkCanvasRect(gl, 4, gl.canvas.height - 8, 2, 2, topColor,
                            "shouldBe " + topColor);
    }

    function runTest(videoElement)
    {
        var red = [255, 0, 0];
        var green = [0, 255, 0];
        runOneIteration(videoElement, false, true, red, green);
        runOneIteration(videoElement, false, false, green, red);
        runOneIteration(videoElement, true, true, red, green);
        runOneIteration(videoElement, true, false, green, red);

        glErrorShouldBe(gl, gl.NO_ERROR, "should be no errors");
        finishTest();
    }

    return init;
}
