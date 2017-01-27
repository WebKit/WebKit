// Test inspired from https://webrtc.github.io/samples/
var localConnection;
var remoteConnection;

function createConnections(setupLocalConnection, setupRemoteConnection) {
    localConnection = new RTCPeerConnection();
    localConnection.onicecandidate = iceCallback1;
    setupLocalConnection(localConnection);

    remoteConnection = new RTCPeerConnection();
    remoteConnection.onicecandidate = iceCallback2;
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

function iceCallback1(event)
{
    if (event.candidate)
        remoteConnection.addIceCandidate(event.candidate).then(onAddIceCandidateSuccess, onAddIceCandidateError);
}

function iceCallback2(event)
{
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
