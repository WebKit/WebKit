"use strict";
if (window.internals)
    window.internals.settings.setWebGLErrorsToConsoleEnabled(false);

const wtu = WebGLTestUtils;
const width = 643;
const verifyWidth = 200;

async function createSourceVideo() {
    const video = document.createElement("video");
    video.srcObject = await navigator.mediaDevices.getUserMedia({ video: { width: { exact: width } } });
    await video.play();
    assert_equals(video.videoWidth, width);
    return video;
}

function createVerifyCanvas(video) {
    const canvas = document.createElement("canvas");
    canvas.width = verifyWidth;
    canvas.height = Math.floor(video.videoHeight / video.videoWidth * verifyWidth);
    return canvas;
}

function createVerifyWebGLState(canvas) {
    const gl = wtu.create3DContext(canvas, { depth: false, stencil: false, antialias: false });
    gl.viewport(0, 0, canvas.width, canvas.height);
    const program = wtu.setupTexturedQuad(gl);
    gl.uniform1i(gl.getUniformLocation(program, "tex"), 0);
    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return [gl, texture];
}

function getFramebufferAsImageData(gl) {
    const canvas = gl.canvas;
    const imageData = {
        width: canvas.width,
        height: canvas.height,
        data: new Uint8Array(canvas.width * canvas.height * 4)
    }
    gl.readPixels(0, 0, canvas.width, canvas.height, gl.RGBA, gl.UNSIGNED_BYTE, imageData.data);
    return imageData;
}

function setupInverseUnitQuadTextureCoordinates(gl, texcoordLocation) {
    texcoordLocation = texcoordLocation || 1;
    const vertexObject = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vertexObject);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
        1.0, 0.0,
        0.0, 0.0,
        0.0, 1.0,
        1.0, 0.0,
        0.0, 1.0,
        1.0, 1.0]), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(texcoordLocation);
    gl.vertexAttribPointer(texcoordLocation, 2, gl.FLOAT, false, 0, 0);
}

async function with2DContext(subcase, video, canvas) {
    const ctx = canvas.getContext("2d");
    let source = video;
    if (subcase.imageBitmap)
        source = await createImageBitmap(source);
    ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
    return ctx.getImageData(0, 0, canvas.width, canvas.height);
}

async function withWebGL(subcase, video, canvas) {
    const [gl, texture] = createVerifyWebGLState(canvas);
    let source = video;
    if (subcase.imageBitmap)
        source = await createImageBitmap(source);
    if (subcase.unpackFlipY)
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
    if (subcase.subImage) {
        const w = video.videoWidth;
        const h = video.videoHeight;
        const pixels = new Uint8Array(w * h * 4);
        pixels.fill(0);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, gl.RGBA, gl.UNSIGNED_BYTE, source);
    } else
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source);

    // UNPACK_FLIP_Y_WEBGL is ignored for ImageBitmaps as per WebGL spec. Test this by
    // rendering normally and expecting normal result.
    const shouldRenderInverted = subcase.unpackFlipY && !subcase.imageBitmap;
    if (shouldRenderInverted) {
        setupInverseUnitQuadTextureCoordinates(gl);
        wtu.clearAndDrawUnitQuad(gl, [0, 0, 0, 255]);
    } else
        wtu.clearAndDrawUnitQuad(gl, [0, 0, 0, 255]);
    return getFramebufferAsImageData(gl);
}

const subcases = [];
const angles = [0, 90, 180]; // FIXME: -90 make setMockCamera... accept signed. https://webkit.org/b/235352
const falseAndTrue = [false, true];
for (const angle of angles) {
    for (const imageBitmap of falseAndTrue)
        subcases.push({ func: with2DContext, angle: angle, imageBitmap: imageBitmap });
}

for (const angle of angles) {
    for (const imageBitmap of falseAndTrue) {
        for (const subImage of falseAndTrue) {
            for (const unpackFlipY of falseAndTrue)
                subcases.push({func: withWebGL, angle: angle, imageBitmap: imageBitmap, subImage: subImage, unpackFlipY: unpackFlipY});
        }
    }
}

function testDescription(subcase) {
    let desc = "func: " + subcase.func.name;
    for (const k in subcase) {
        if (k === "func")
            continue;
        desc += `, ${k}: ${subcase[k]}`;
    }
    return desc;
}

async function testUserMediaToCanvas(t, subcase) {
    const desc = testDescription(subcase);
    const video = await createSourceVideo();
    const debuge = document.getElementById("debuge");
    debuge.append(video);
    const realVideoSize = [video.videoWidth, video.videoHeight];
    t.add_cleanup(async () => {
        // Reset orientation for the next test by going to 0.
        if (subcase.angle == 180) {
            // 180 -> 0 cannot detect rotation via video size change. Go via 90.
            const [angle, videoSize] = setMockCameraImageOrientation(90, realVideoSize);
            await waitForVideoSize(video, videoSize[0], videoSize[1]);
        }
        setMockCameraImageOrientation(0);
        await waitForVideoSize(video, realVideoSize[0], realVideoSize[1]);
        debuge.removeChild(video);
        if (video.srcObject)
            video.srcObject.getTracks().forEach(t => t.stop());
        video.srcObject = null;
    });

    if (subcase.angle == 180) {
        // 0 -> 180 cannot detect rotation via video size change. Go via 90.
        const [angle, videoSize] = setMockCameraImageOrientation(90, realVideoSize);
        await waitForVideoSize(video, videoSize[0], videoSize[1]);
    }
    const [angle, videoSize] = setMockCameraImageOrientation(subcase.angle, realVideoSize);
    await waitForVideoSize(video, videoSize[0], videoSize[1]);

    const canvas = createVerifyCanvas(video);
    debuge.appendChild(canvas);
    t.add_cleanup(async () => debuge.removeChild(canvas));
    const imageData = await subcase.func(subcase, video, canvas);
    try {
        assertImageDataContainsMockCameraImage(imageData, angle, desc);
    } catch (e) {
        // Use this for debugging:
        //await waitFor(10000);
        throw e;
    }
}

function createUserMediaToCanvasTests(slice, slices) {
    const sliceSize = Math.floor(subcases.length / slices);
    const first = sliceSize * slice;
    for (const subcase of subcases.slice(first, first + sliceSize))
        promise_test(async t => { await testUserMediaToCanvas(t, subcase); }, testDescription(subcase));
}
