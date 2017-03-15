// Test inspired from https://webrtc.github.io/samples/
var localConnection;
var remoteConnection;

function createConnections(setupLocalConnection, setupRemoteConnection, filterOutICECandidate) {
    localConnection = new RTCPeerConnection();
    localConnection.onicecandidate = (event) => { iceCallback1(event, filterOutICECandidate) };
    setupLocalConnection(localConnection);

    remoteConnection = new RTCPeerConnection();
    remoteConnection.onicecandidate = (event) => { iceCallback2(event, filterOutICECandidate) };
    setupRemoteConnection(remoteConnection);

    localConnection.createOffer().then(gotDescription1, onCreateSessionDescriptionError);

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

function gotDescription1(desc)
{
    localConnection.setLocalDescription(desc);
    remoteConnection.setRemoteDescription(desc);
    remoteConnection.createAnswer().then(gotDescription2, onCreateSessionDescriptionError);
}

function gotDescription2(desc)
{
    remoteConnection.setLocalDescription(desc);
    localConnection.setRemoteDescription(desc);
}

function iceCallback1(event, filterOutICECandidate)
{
    if (!event.candidate)
        return;

    if (filterOutICECandidate && filterOutICECandidate(event.candidate))
        return;

    remoteConnection.addIceCandidate(event.candidate).then(onAddIceCandidateSuccess, onAddIceCandidateError);
}

function iceCallback2(event, filterOutICECandidate)
{
    if (!event.candidate)
        return;

    if (filterOutICECandidate && filterOutICECandidate(event.candidate))
        return;

    if (event.candidate)
        localConnection.addIceCandidate(event.candidate).then(onAddIceCandidateSuccess, onAddIceCandidateError);
}

function onAddIceCandidateSuccess()
{
}

function onAddIceCandidateError(error)
{
    assert_unreached();
}

function analyseAudio(stream, duration)
{
    return new Promise((resolve, reject) => {
        var context = new webkitAudioContext();
        var sourceNode = context.createMediaStreamSource(stream);
        var analyser = context.createAnalyser();
        var gain = context.createGain();

        var results = { heardHum: false, heardBip: false, heardBop: false };

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
                return bin < freqDomain.length && freqDomain[bin] >= 150;
           };

           if (!results.heardHum)
                results.heardHum = hasFrequency(150);

           if (!results.heardBip)
               results.heardBip = hasFrequency(1500);

           if (!results.heardBop)
                results.heardBop = hasFrequency(500);

            if (results.heardHum && results.heardBip && results.heardBop)
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
