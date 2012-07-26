description("Tests PeerConnection00::addStream().");

var pc = new webkitPeerConnection00("STUN some.server.com", function() {});

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
try {
    pc.addStream({});
} catch(e) {
    testPassed('pc.addStream({}) threw' + e);
}
try {
    pc.addStream(42);
} catch(e) {
    testPassed('pc.addStream(42) threw' + e);
}
try {
    pc.addStream(Infinity);
} catch(e) {
    testPassed('pc.addStream(Infinity) threw' + e);
}
try {
    pc.addStream(-Infinity);
} catch(e) {
    testPassed('pc.addStream(-Infinity) threw' + e);
}

window.successfullyParsed = true;

