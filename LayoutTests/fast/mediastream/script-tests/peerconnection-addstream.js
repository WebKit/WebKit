description("Tests PeerConnection::addStream().");

var pc = new webkitPeerConnection("STUN some.server.com", function() {});

try {
    pc.addStream();
} catch(e) {
    testPassed('pc.addStream() threw ' + e);
}

try {
    pc.addStream(undefined);
} catch(e) {
    testPassed('pc.addStream(undefined) threw ' + e);
}

try {
    pc.addStream(null);
} catch(e) {
    testPassed('pc.addStream(null) threw ' + e);
}

try {
    pc.addStream(new Array());
} catch(e) {
    testPassed('pc.addStream(new Array()) threw ' + e);
}

finishJSTest();

window.successfullyParsed = true;

