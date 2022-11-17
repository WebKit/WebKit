// Test inspired from https://webrtc.github.io/samples/
var localConnection;
var remoteConnection;

function createConnections(setupLocalConnection, setupRemoteConnection, options = { }) {
    localConnection = new RTCPeerConnection();
    remoteConnection = new RTCPeerConnection();
    remoteConnection.onicecandidate = (event) => { iceCallback2(event, options.filterOutICECandidate, remoteConnection) };

    localConnection.onicecandidate = (event) => { iceCallback1(event, options.filterOutICECandidate, localConnection) };

    Promise.resolve(setupLocalConnection(localConnection)).then(() => {
        return Promise.resolve(setupRemoteConnection(remoteConnection));
    }).then(() => {
        localConnection.createOffer().then((desc) => gotDescription1(desc, options), onCreateSessionDescriptionError);
    });

    return [localConnection, remoteConnection]
}

function closeConnections()
{
    localConnection.close();
    remoteConnection.close();
}

function onCreateSessionDescriptionError(error)
{
    assert_unreached();
}

function gotDescription1(desc, options)
{
    if (options.observeOffer) {
        const result = options.observeOffer(desc);
        if (result)
            desc = result;
    }

    localConnection.setLocalDescription(desc);
    remoteConnection.setRemoteDescription(desc).then(() => {
        remoteConnection.createAnswer().then((desc) => gotDescription2(desc, options), onCreateSessionDescriptionError);
    });
}

function gotDescription2(desc, options)
{
    if (options.observeAnswer)
        options.observeAnswer(desc);

    remoteConnection.setLocalDescription(desc);
    localConnection.setRemoteDescription(desc);
}

function iceCallback1(event, filterOutICECandidate, connection)
{
    if (filterOutICECandidate && filterOutICECandidate(event.candidate, connection))
        return;

    remoteConnection.addIceCandidate(event.candidate).then(onAddIceCandidateSuccess, onAddIceCandidateError);
}

function iceCallback2(event, filterOutICECandidate, connection)
{
    if (filterOutICECandidate && filterOutICECandidate(event.candidate, connection))
        return;

    localConnection.addIceCandidate(event.candidate).then(onAddIceCandidateSuccess, onAddIceCandidateError);
}

function onAddIceCandidateSuccess()
{
}

function onAddIceCandidateError(error)
{
    console.log("addIceCandidate error: " + error)
    assert_unreached();
}

async function renegotiate(pc1, pc2)
{
    let d = await pc1.createOffer();
    await pc1.setLocalDescription(d);
    await pc2.setRemoteDescription(d);
    d = await pc2.createAnswer();
    await pc1.setRemoteDescription(d);
    await pc2.setLocalDescription(d);
}

function analyseAudio(stream, duration, context)
{
    return new Promise((resolve, reject) => {
        var sourceNode = context.createMediaStreamSource(stream);

        var analyser = context.createAnalyser();
        var gain = context.createGain();

        var results = { heardHum: false, heardBip: false, heardBop: false, heardNoise: false };

        analyser.fftSize = 2048;
        analyser.smoothingTimeConstant = 0;
        analyser.minDecibels = -100;
        analyser.maxDecibels = 0;
        gain.gain.value = 0;

        sourceNode.connect(analyser);
        analyser.connect(gain);
        gain.connect(context.destination);

       function analyse() {
           var freqDomain = new Uint8Array(analyser.frequencyBinCount);
           analyser.getByteFrequencyData(freqDomain);

           var hasFrequency = expectedFrequency => {
                var bin = Math.floor(expectedFrequency * analyser.fftSize / context.sampleRate);
                return bin < freqDomain.length && freqDomain[bin] >= 100;
           };

           if (!results.heardHum)
                results.heardHum = hasFrequency(150);

           if (!results.heardBip)
               results.heardBip = hasFrequency(1500);

           if (!results.heardBop)
                results.heardBop = hasFrequency(500);

           if (!results.heardNoise)
                results.heardNoise = hasFrequency(3000);

           if (results.heardHum && results.heardBip && results.heardBop && results.heardNoise)
                done();
        };

       function done() {
            clearTimeout(timeout);
            clearInterval(interval);
            resolve(results);
       }

        var timeout = setTimeout(done, 3 * duration);
        var interval = setInterval(analyse, duration / 30);
        analyse();
    });
}

function waitFor(duration)
{
    return new Promise((resolve) => setTimeout(resolve, duration));
}

async function waitForVideoSize(video, width, height, count)
{
    if (video.requestVideoFrameCallback) {
        const frameMetadata = await new Promise(resolve => video.requestVideoFrameCallback((now, metadata) => {
            resolve(metadata);
        }));

        if (frameMetadata.width === width && frameMetadata.height === height)
            return Promise.resolve("video has expected size");
    } else if (video.videoWidth === width && video.videoHeight === height)
        return Promise.resolve("video has expected size");

    if (count === undefined)
        count = 0;
    if (++count > 20)
        return Promise.reject("waitForVideoSize timed out, expected " + width + "x"+ height + " but got " + video.videoWidth + "x" + video.videoHeight);

    await waitFor(100);
    return waitForVideoSize(video, width, height, count);
}

async function doHumAnalysis(stream, expected)
{
    var context = new AudioContext();
    for (var cptr = 0; cptr < 20; cptr++) {
        var results = await analyseAudio(stream, 200, context);
        if (results.heardHum === expected) {
            await context.close();
            return true;
        }
        await waitFor(50);
    }
    await context.close();
    return false;
}

function isVideoBlack(canvas, video, startX, startY, grabbedWidth, grabbedHeight)
{
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;
    if (!grabbedHeight) {
        startX = 10;
        startY = 10;
        grabbedWidth = canvas.width - 20;
        grabbedHeight = canvas.height - 20;
    }

    canvas.getContext('2d').drawImage(video, 0, 0, canvas.width, canvas.height);

    imageData = canvas.getContext('2d').getImageData(startX, startY, grabbedWidth, grabbedHeight);
    data = imageData.data;
    for (var cptr = 0; cptr < grabbedWidth * grabbedHeight; ++cptr) {
        // Approximatively black pixels.
        if (data[4 * cptr] > 30 || data[4 * cptr + 1] > 30 || data[4 * cptr + 2] > 30)
            return false;
    }
    return true;
}

async function checkVideoBlack(expected, canvas, video, errorMessage, counter)
{
    if (isVideoBlack(canvas, video) === expected)
        return Promise.resolve();

    if (counter === undefined)
        counter = 400;
    if (counter === 0) {
        if (!errorMessage)
            errorMessage = "checkVideoBlack timed out expecting " + expected;
        return Promise.reject(errorMessage);
    }

    await waitFor(50);
    return checkVideoBlack(expected, canvas, video, errorMessage, --counter);
}

function setCodec(sdp, codec)
{
    return sdp.split('\r\n').filter(line => {
        return line.indexOf('a=fmtp') === -1 && line.indexOf('a=rtcp-fb') === -1 && (line.indexOf('a=rtpmap') === -1 || line.indexOf(codec) !== -1);
    }).join('\r\n');
}

async function getTypedStats(connection, type)
{
    const report = await connection.getStats();
    var stats;
    report.forEach((statItem) => {
        if (statItem.type === type)
            stats = statItem;
    });
    return stats;
}

function getReceivedTrackStats(connection)
{
    return connection.getStats().then((report) => {
        var stats;
        report.forEach((statItem) => {
            if (statItem.type === "track") {
                stats = statItem;
            }
        });
        return stats;
    });
}

async function computeFrameRate(stream, video)
{
    if (window.internals) {
        internals.observeMediaStreamTrack(stream.getVideoTracks()[0]);
        await new Promise(resolve => setTimeout(resolve, 1000));
        return internals.trackVideoSampleCount;
    }

    let connection;
    video.srcObject = await new Promise((resolve, reject) => {
        createConnections((firstConnection) => {
            firstConnection.addTrack(stream.getVideoTracks()[0], stream);
        }, (secondConnection) => {
            connection = secondConnection;
            secondConnection.ontrack = (trackEvent) => {
                resolve(trackEvent.streams[0]);
            };
        });
        setTimeout(() => reject("Test timed out"), 5000);
    });

    await video.play();

    const stats1 = await getReceivedTrackStats(connection);
    await new Promise(resolve => setTimeout(resolve, 1000));
    const stats2 = await getReceivedTrackStats(connection);
    return (stats2.framesReceived - stats1.framesReceived) * 1000 / (stats2.timestamp - stats1.timestamp);
}

function setH264BaselineCodec(sdp)
{
    const lines = sdp.split('\r\n');
    const h264Lines = lines.filter(line => line.indexOf("a=fmtp") === 0 && line.indexOf("42e01f") !== -1);
    const baselineNumber = h264Lines[0].substring(6).split(' ')[0];
    return lines.filter(line => {
        return (line.indexOf('a=fmtp') === -1 && line.indexOf('a=rtcp-fb') === -1 && line.indexOf('a=rtpmap') === -1) || line.indexOf(baselineNumber) !== -1;
    }).join('\r\n');
}

function setH264HighCodec(sdp)
{
    const lines = sdp.split('\r\n');
    const h264Lines = lines.filter(line => line.indexOf("a=fmtp") === 0 && line.indexOf("640c1f") !== -1);
    const baselineNumber = h264Lines[0].substring(6).split(' ')[0];
    return lines.filter(line => {
        return (line.indexOf('a=fmtp') === -1 && line.indexOf('a=rtcp-fb') === -1 && line.indexOf('a=rtpmap') === -1) || line.indexOf(baselineNumber) !== -1;
    }).join('\r\n');
}

const testColors = {
    white: [ 255, 255, 255, 255 ],
    yellow: [ 255, 255, 0, 255 ],
    cyan: [ 0, 255, 255, 255 ],
    lightGreen: [ 0, 128, 0, 255 ],
};

// Sets the camera image orientation if running on test runner.
// angle: orientation angle of the camera image in degrees
function setMockCameraImageOrientation(angle, videoSize) {
    if ([0, 90, 180, 270].indexOf(angle) == -1)
        throw "invalid angle";
    if (window.testRunner) {
        testRunner.setMockCameraOrientation(angle);
        if (videoSize && (angle == 90 || angle == 270))
            videoSize = [videoSize[1], videoSize[0]];
        return [angle, videoSize];
    }
    return [0, videoSize];
}

// Returns Uint8Array[4] of RGBA color.
// p: [x, y] of 0..1 range.
function getImageDataPixel(imageData, p)
{
    let xi = Math.floor(p[0] * imageData.width);
    let yi = Math.floor(p[1] * imageData.height);
    let i = (yi * imageData.width + xi) * 4;
    return imageData.data.slice(i, i + 4);
}

// Asserts that ImageData instance contains mock camera image rendered by MiniBrowser and WebKitTestRunner.
// Obtain full camera image of size `width`:
//  await navigator.mediaDevices.getUserMedia({ video: { width: { exact: width } } });
function assertImageDataContainsMockCameraImage(imageData, angle, desc)
{
    angle = angle || 0;
    desc = desc || "";

    function rotatePoint(p) {
        let a = angle;
        let n = [ p[0], p[1] ];
        while (a > 0) {
            n = [ 1.0 - n[1], n[0] ];
            a -= 90;
        }
        return n;
    }

    const whitePoint = rotatePoint([ 0.04, 0.7 ]);
    const yellowPoint = rotatePoint([ 0.08, 0.7 ]);
    const cyanPoint = rotatePoint([ 0.12, 0.7 ]);
    const lightGreenPoint = rotatePoint([ 0.16, 0.7 ]);

    let err = 11;
    assert_array_approx_equals(getImageDataPixel(imageData, whitePoint), testColors.white, err, "white rect not found " + desc);
    assert_array_approx_equals(getImageDataPixel(imageData, yellowPoint), testColors.yellow, err, "yellow rect not found" + desc);
    assert_array_approx_equals(getImageDataPixel(imageData, cyanPoint), testColors.cyan, err, "cyan rect not found " + desc);
    assert_array_approx_equals(getImageDataPixel(imageData, lightGreenPoint), testColors.lightGreen, err, "light green rect not found " + desc);
}

// Draws a canvas test pattern with WebGL.
// Can be checked with `assertImageSourceContainsCanvasTestPattern()`.
// Pattern is four colors, it can detect flips and rotations.
// `patternNumber` can be used in to test multiple subsequent frames (pass in frame number).
// Pattern is four boxes with color cycled based on patternNumber % 4.
function drawCanvasTestPatternWebGL(canvas, patternNumber)
{
    patternNumber = patternNumber || 0;
    const gl = canvas.getContext("webgl", { depth: false, stencil: false, antialias: false });
    gl.enable(gl.SCISSOR_TEST);
    const boxSize = [ canvas.width, canvas.height ];
    const boxes = [
        [0.0, 0.5, 0.5, 1.0],
        [0.5, 0.5, 1.0, 1.0],
        [0.5, 0.0, 1.0, 0.5],
        [0.0, 0.0, 0.5, 0.5],
    ];
    const boxColors = Object.values(testColors);
    for (let n = 0; n < 4; ++n) {
        const i = (n + patternNumber) % boxes.length;
        const color = boxColors[i];
        const box = boxes[n];
        gl.scissor(box[0] * boxSize[0], box[1] * boxSize[1], box[2] * boxSize[0], box[3] * boxSize[1]);
        gl.clearColor(color[0] / 255., color[1] / 255, color[2] / 255., color[3] / 255.);
        gl.clear(gl.COLOR_BUFFER_BIT);
    }
}

function drawCanvasTestPattern2D(canvas, patternNumber)
{
    patternNumber = patternNumber || 0;
    const context = canvas.getContext("2d");
    const boxSize = [ canvas.width, canvas.height ];
    const boxes = [
        [0.0, 0.0, 0.5, 0.5],
        [0.5, 0.0, 1.0, 0.5],
        [0.5, 0.5, 1.0, 1.0],
        [0.0, 0.5, 0.5, 1.0],
    ];
    const boxColors = Object.values(testColors);
    for (let n = 0; n < 4; ++n) {
        const i = (n + patternNumber) % boxes.length;
        const color = boxColors[i];
        const box = boxes[n];

        context.fillStyle = `rgb(${color[0]}, ${color[1]}, ${color[2]})`;
        context.fillRect(box[0] * boxSize[0], box[1] * boxSize[1], box[2] * boxSize[0], box[3] * boxSize[1]);
    }
}


function assertImageSourceContainsCanvasTestPattern(imageSource, patternNumber, desc, err)
{
    patternNumber = patternNumber || 0;
    desc = desc || "";
    const verifyWidth = 300;
    // FIXME: canvas-to-peer-connection on Catalina-WK2 would fail with 0 -> 8 and 255 -> 240 for some reason.
    // https://bugs.webkit.org/show_bug.cgi?id=235708
    if (!err)
        err = 25;
    let imageSourceSize;
    let imageSourceHeight;
    if (imageSource instanceof HTMLVideoElement)
        imageSourceSize = [imageSource.videoWidth, imageSource.videoHeight];
    else
        imageSourceSize = [imageSource.width, imageSource.height];

    const canvas = document.createElement("canvas");
    const debuge = document.getElementById("debuge");
    if (debuge)
        debuge.appendChild(canvas);

    canvas.width = verifyWidth;
    canvas.height = (verifyWidth / imageSourceSize[0]) * imageSourceSize[1];
    const ctx = canvas.getContext("2d");
    ctx.drawImage(imageSource, 0, 0, canvas.width, canvas.height);
    const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
    const pointColors = Object.values(testColors);
    const points = [
        [ 0.1, 0.1 ],
        [ 0.9, 0.1 ],
        [ 0.9, 0.9 ],
        [ 0.1, 0.9 ],
    ];
    for (let n = 0; n < 4; ++n) {
        let i = (n + patternNumber) % points.length;
        let color = pointColors[i];
        let point = points[n];
        assert_array_approx_equals(getImageDataPixel(imageData, point), color, err, `rect ${color.join(",")} at ${point.join(",")} not found ${desc}`);
    }
    if (debuge)
        debuge.removeChild(canvas);
}
