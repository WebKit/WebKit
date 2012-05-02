description("Tests PeerConnection00 related attributes according to http://www.w3.org/TR/webrtc/");

pc = new webkitPeerConnection00("STUN NONE", function() {});

// Methods
shouldBeTrue("typeof pc.addStream === 'function'");
shouldBeTrue("typeof pc.removeStream === 'function'");
shouldBeTrue("typeof pc.close === 'function'");

// PeerConnection readyState
shouldBeTrue("pc.NEW === 0");
shouldBeTrue("pc.OPENING === 1");
shouldBeTrue("pc.ACTIVE === 2");
shouldBeTrue("pc.CLOSED === 3");

// IceState
shouldBeTrue("pc.ICE_GATHERING === 0x100");
shouldBeTrue("pc.ICE_WAITING === 0x200");
shouldBeTrue("pc.ICE_CHECKING === 0x300");
shouldBeTrue("pc.ICE_CONNECTED === 0x400");
shouldBeTrue("pc.ICE_COMPLETED === 0x500");
shouldBeTrue("pc.ICE_FAILED === 0x600");
shouldBeTrue("pc.ICE_CLOSED === 0x700");

// SDP state
shouldBeTrue("pc.SDP_OFFER === 0x100");
shouldBeTrue("pc.SDP_PRANSWER === 0x200");
shouldBeTrue("pc.SDP_ANSWER ===  0x300");

//MediaStream[] attribute
shouldBeTrue("typeof pc.localStreams === 'object'");
shouldBeTrue("typeof pc.remoteStreams === 'object'");

//callback function definition
shouldBeTrue("typeof pc.onaddstream === 'object'");
shouldBeTrue("typeof pc.onremovestream === 'object'");
shouldBeTrue("typeof pc.onconnecting === 'object'");
shouldBeTrue("typeof pc.onopen === 'object'");

window.jsTestIsAsync = false;
