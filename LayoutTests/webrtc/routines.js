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
