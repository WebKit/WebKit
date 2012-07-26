description("Tests PeerConnection00::removeStream().");

var pc = new webkitPeerConnection00("STUN some.server.com", function() {});

shouldThrow('pc.removeStream();');
shouldThrow('pc.removeStream(undefined);');
shouldThrow('pc.removeStream(null);');
shouldThrow('pc.removeStream(new Array());');
shouldThrow('pc.removeStream({});');
shouldThrow('pc.removeStream(42);');
shouldThrow('pc.removeStream(Infinity);');
shouldThrow('pc.removeStream(-Infinity);');

window.successfullyParsed = true;

