function with2DContext(subcase, canvas, patternNumber)
{
    drawCanvasTestPattern2D(canvas, patternNumber);
}

function withWebGL(subcase, canvas, patternNumber)
{
    drawCanvasTestPatternWebGL(canvas, patternNumber);
}

var useVP8 = false;
async function createLocalPeerConnectionStream(stream)
{
    return new Promise((resolve, reject) => {
        createConnections((firstConnection) => {
            firstConnection.addTrack(stream.getVideoTracks()[0], stream);
            if (useVP8)
                firstConnection.getTransceivers()[0].setCodecPreferences([{mimeType: "video/VP8", clockRate: 90000}]);
        }, (secondConnection) => {
            secondConnection.ontrack = (trackEvent) => {
                resolve(trackEvent.streams[0]);
            };
        });
    });
}

function waitForVideoFrame(video)
{
    return new Promise((resolve) => {
        video.requestVideoFrameCallback((now, metadata) => {
            resolve(now, metadata);
        });
    });
}

const width = 500;
const height = 500;

async function waitForVideoFrameSize(video, width, height, counter)
{
    if (!counter)
        counter = 0;
    else if (counter > 100)
        return Promise.reject("waitForVideoFrameSize timed out");

    const result = await new Promise((resolve, reject) => {
        video.requestVideoFrameCallback((now, metadata) => {
            resolve(metadata.width === width && metadata.height === height);
        });
        setTimeout(() => reject("video.requestVideoFrameCallback timed out"), 5000);
    });
    if (result)
        return;

    return waitForVideoFrameSize(video, width, height, ++counter);
}

let pixelColorTestError = 25;
async function drawAndValidate(subcase, canvas, video, counter, stepName)
{
    // First change canvas width and wait for width change to clear previous frames.
    canvas.width = canvas.width + 500;
    canvas.height = canvas.height + 500;

    let counter1 = counter;
    let id = setInterval(() => { subcase.draw(subcase, canvas, counter1++)}, 100);
    await waitForVideoFrameSize(video, canvas.width, canvas.height);
    clearInterval(id);

    // Get back canvas size to normal and wait for width change to get test frames.
    canvas.width = canvas.width - 500;
    canvas.height = canvas.height - 500;
    id = setInterval(() => { subcase.draw(subcase, canvas, counter)}, 100);
    subcase.draw(subcase, canvas, counter);
    await waitForVideoFrameSize(video, canvas.width, canvas.height);
    clearInterval(id);

    assertImageSourceContainsCanvasTestPattern(video, counter, stepName, pixelColorTestError);
}

async function testCanvasToPeerConnection(t, subcase)
{
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    debuge.appendChild(canvas);
    const localStream = canvas.captureStream();
    const remoteStream = await createLocalPeerConnectionStream(localStream);

    const video = document.createElement("video");
    video.autoplay = true;
    video.controls = true;
    debuge.appendChild(video);
    t.add_cleanup(async () => debuge.removeChild(video));
    video.srcObject = remoteStream;

    for (let i = 0; i < 5; ++i)
        await drawAndValidate(subcase, canvas, video, 1, `base case: ${i}`);

    for (let i = 0; i < 5; ++i)
        await drawAndValidate(subcase, canvas, video, i, `iteration: ${i}`);
}

