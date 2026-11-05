const workerCode = `
onmessage = async (event) => {
    const reader = new MediaStreamTrackProcessor({ track: event.data }).readable.getReader();
    const { value } = await reader.read();
    reader.cancel();
    event.data.stop();
    postMessage(value, [value]);
};
`;

// Gets a camera VideoFrame like hang.live does: MediaStreamTrackProcessor in a worker, then re-stamped in the window.
async function getCameraVideoFrame(track)
{
    const worker = new Worker(URL.createObjectURL(new Blob([workerCode], { type: 'text/javascript' })));
    const workerTrack = track.clone();
    const frame = await new Promise(resolve => {
        worker.onmessage = (event) => resolve(event.data);
        worker.postMessage(workerTrack, [workerTrack]);
    });
    worker.terminate();
    const restampedFrame = new VideoFrame(frame, { timestamp: frame.timestamp + 1 });
    frame.close();
    return restampedFrame;
}

function readWebGLPixels(frame, path, width, height)
{
    webglCanvas.width = width;
    webglCanvas.height = height;
    const gl = webglCanvas.getContext('webgl');
    internals.setWebGLVideoUploadPath(gl, path);

    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, frame);
    assert_equals(gl.getError(), gl.NO_ERROR, 'texImage2D');
    assert_equals(internals.webglLastVideoUpload(gl)?.path, path, 'upload path');

    const framebuffer = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);
    assert_equals(gl.checkFramebufferStatus(gl.FRAMEBUFFER), gl.FRAMEBUFFER_COMPLETE, 'framebuffer');

    const pixels = new Uint8Array(width * height * 4);
    gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    assert_equals(gl.getError(), gl.NO_ERROR, 'readPixels');

    gl.deleteFramebuffer(framebuffer);
    gl.deleteTexture(texture);
    internals.setWebGLVideoUploadPath(gl, 'default');
    return pixels;
}

function readReferencePixels(image, width, height)
{
    referenceCanvas.width = width;
    referenceCanvas.height = height;
    const context = referenceCanvas.getContext('2d');
    context.drawImage(image, 0, 0, width, height);
    return context.getImageData(0, 0, width, height).data;
}

function countDifferentPixels(data1, data2)
{
    const error = 10;
    let counter = 0;
    for (let i = 0; i < data1.length; i += 4) {
        if (Math.abs(data1[i] - data2[i]) >= error || Math.abs(data1[i + 1] - data2[i + 1]) >= error || Math.abs(data1[i + 2] - data2[i + 2]) >= error)
            ++counter;
    }
    return counter;
}

function validateRotation(isRotationEnabled, rotation, path)
{
    promise_test(async (test) => {
        if (!window.testRunner || !window.internals)
            return;

        assert_equals('rotation' in VideoFrame.prototype, isRotationEnabled, 'WebCodecsVideoFrameRotationEnabled');

        testRunner.setMockCameraOrientation(rotation);
        const stream = await navigator.mediaDevices.getUserMedia({ video: { width: 640, height: 480, frameRate: 5 } });
        const [track] = stream.getVideoTracks();
        test.add_cleanup(() => track.stop());

        const frame = await getCameraVideoFrame(track);
        test.add_cleanup(() => frame.close());

        if (isRotationEnabled)
            assert_equals(frame.rotation, rotation, 'rotation');

        // The display size is rotated only when WebCodecsVideoFrameRotationEnabled is on.
        const isRotated = rotation === 90 || rotation === 270;
        const isDisplaySizeRotated = isRotationEnabled && isRotated;
        assert_equals(frame.codedWidth, 640, 'codedWidth');
        assert_equals(frame.codedHeight, 480, 'codedHeight');
        assert_equals(frame.displayWidth, isDisplaySizeRotated ? 480 : 640, 'displayWidth');
        assert_equals(frame.displayHeight, isDisplaySizeRotated ? 640 : 480, 'displayHeight');

        // ImageCapture applies the rotation.
        const image = await new ImageCapture(track).grabFrame();
        const width = isRotated ? 480 : 640;
        const height = isRotated ? 640 : 480;
        assert_equals(image.width, width, 'grabFrame width');
        assert_equals(image.height, height, 'grabFrame height');

        const referencePixels = readReferencePixels(image, width, height);
        const webglPixels = readWebGLPixels(frame, path, width, height);

        // The mock camera content moves, allow some differences between the two frames.
        assert_less_than(countDifferentPixels(referencePixels, webglPixels), width * height / 25);
    }, `WebGL ${path} upload of a ${rotation} degrees rotated camera VideoFrame`);
}

function validateRotations(isRotationEnabled)
{
    for (const path of ['hardware', 'software']) {
        for (const rotation of [0, 90, 180, 270])
            validateRotation(isRotationEnabled, rotation, path);
    }
}
