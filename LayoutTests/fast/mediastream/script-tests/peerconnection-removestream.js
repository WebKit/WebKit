description("Tests DeprecatedPeerConnection::removeStream().");

var pc = new webkitDeprecatedPeerConnection("STUN some.server.com", function() {});

try {
    pc.removeStream();
} catch(e) {
    testPassed('pc.removeStream() threw ' + e);
}

try {
    pc.removeStream(undefined);
} catch(e) {
    testPassed('pc.removeStream(undefined) threw ' + e);
}

try {
    pc.removeStream(null);
} catch(e) {
    testPassed('pc.removeStream(null) threw ' + e);
}

try {
    pc.removeStream(new Array());
} catch(e) {
    testPassed('pc.removeStream(new Array()) threw ' + e);
}
try {
    pc.removeStream({});
} catch(e) {
    testPassed('pc.removeStream({}) threw' + e);
}
try {
    pc.removeStream(42);
} catch(e) {
    testPassed('pc.removeStream(42) threw' + e);
}
try {
    pc.removeStream(Infinity);
} catch(e) {
    testPassed('pc.removeStream(Infinity) threw' + e);
}
try {
    pc.removeStream(-Infinity);
} catch(e) {
    testPassed('pc.removeStream(-Infinity) threw' + e);
}

window.successfullyParsed = true;

