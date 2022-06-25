// Test inspired from https://webrtc.github.io/samples/
var localConnection;
var remoteConnection;

function createConnections(setupLocalConnection, setupRemoteConnection, options = { }) {
    localConnection = new RTCPeerConnection();
    remoteConnection = new RTCPeerConnection();
    remoteConnection.onicecandidate = (event) => { iceCallback2(event, options.filterOutICECandidate) };

    localConnection.onicecandidate = (event) => { iceCallback1(event, options.filterOutICECandidate) };

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

function iceCallback1(event, filterOutICECandidate)
{
    if (filterOutICECandidate && filterOutICECandidate(event.candidate))
        return;

    remoteConnection.addIceCandidate(event.candidate).then(onAddIceCandidateSuccess, onAddIceCandidateError);
}

function iceCallback2(event, filterOutICECandidate)
{
    if (filterOutICECandidate && filterOutICECandidate(event.candidate))
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

function waitForVideoSize(video, width, height, count)
{
    if (video.videoWidth === width && video.videoHeight === height)
        return Promise.resolve("video has expected size");

    if (count === undefined)
        count = 0;
    if (++count > 20)
        return Promise.reject("waitForVideoSize timed out, expected " + width + "x"+ height + " but got " + video.videoWidth + "x" + video.videoHeight);

    return waitFor(100).then(() => {
        return waitForVideoSize(video, width, height, count);
    });
}

async function doHumAnalysis(stream, expected)
{
    var context = new AudioContext();
    for (var cptr = 0; cptr < 20; cptr++) {
        var results = await analyseAudio(stream, 200, context);
        if (results.heardHum === expected)
            return true;
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
        counter = 0;
    if (counter > 400) {
        if (!errorMessage)
            errorMessage = "checkVideoBlack timed out expecting " + expected;
        return Promise.reject(errorMessage);
    }

    await waitFor(50);
    return checkVideoBlack(expected, canvas, video, errorMessage, ++counter);
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
