'use strict'

/*
 *  Helper Methods for testing the following methods in RTCPeerConnection:
 *    createOffer
 *    createAnswer
 *    setLocalDescription
 *    setRemoteDescription
 *
 *  This file offers the following features:
 *    SDP similarity comparison
 *    Generating offer/answer using anonymous peer connection
 *    Test signalingstatechange event
 *    Test promise that never resolve
 */

const audioLineRegex = /\r\nm=audio.+\r\n/g;
const videoLineRegex = /\r\nm=video.+\r\n/g;
const applicationLineRegex = /\r\nm=application.+\r\n/g;

function countLine(sdp, regex) {
  const matches = sdp.match(regex);
  if(matches === null) {
    return 0;
  } else {
    return matches.length;
  }
}

function countAudioLine(sdp) {
  return countLine(sdp, audioLineRegex);
}

function countVideoLine(sdp) {
  return countLine(sdp, videoLineRegex);
}

function countApplicationLine(sdp) {
  return countLine(sdp, applicationLineRegex);
}

function similarMediaDescriptions(sdp1, sdp2) {
  if(sdp1 === sdp2) {
    return true;
  } else if(
    countAudioLine(sdp1) !== countAudioLine(sdp2) ||
    countVideoLine(sdp1) !== countVideoLine(sdp2) ||
    countApplicationLine(sdp1) !== countApplicationLine(sdp2))
  {
    return false;
  } else {
    return true;
  }
}

// Assert that given object is either an
// RTCSessionDescription or RTCSessionDescriptionInit
function assert_is_session_description(sessionDesc) {
  if(sessionDesc instanceof RTCSessionDescription) {
    return;
  }

  assert_not_equals(sessionDesc, undefined,
    'Expect session description to be defined, but got undefined');

  assert_true(typeof(sessionDesc) === 'object',
    'Expect sessionDescription to be either a RTCSessionDescription or an object');

  assert_true(typeof(sessionDesc.type) === 'string',
    'Expect sessionDescription.type to be a string');

  assert_true(typeof(sessionDesc.type) === 'string',
    'Expect sessionDescription.sdp to be a string');
}


// We can't do string comparison to the SDP content,
// because RTCPeerConnection may return SDP that is
// slightly modified or reordered from what is given
// to it due to ICE candidate events or serialization.
// Instead, we create SDP with different number of media
// lines, and if the SDP strings are not the same, we
// simply count the media description lines and if they
// are the same, we assume it is the same.
function isSimilarSessionDescription(sessionDesc1, sessionDesc2) {
  assert_is_session_description(sessionDesc1);
  assert_is_session_description(sessionDesc2);

  if(sessionDesc1.type !== sessionDesc2.type) {
    return false;
  } else {
    return similarMediaDescriptions(sessionDesc1.sdp, sessionDesc2.sdp);
  }
}

function assert_session_desc_equals(sessionDesc1, sessionDesc2) {
  assert_true(isSimilarSessionDescription(sessionDesc1, sessionDesc2),
    'Expect both session descriptions to have the same count of media lines');
}

function assert_session_desc_not_equals(sessionDesc1, sessionDesc2) {
  assert_false(isSimilarSessionDescription(sessionDesc1, sessionDesc2),
    'Expect both session descriptions to have different count of media lines');
}

// Helper function to generate offer using a freshly created RTCPeerConnection
// object with any audio, video, data media lines present
function generateOffer(options={}) {
  const {
    audio=false,
    video=false,
    data=false
  } = options;

  const pc = new RTCPeerConnection();

  if(data) {
    pc.createDataChannel('test');
  }

  return pc.createOffer({
    offerToReceiveAudio: audio,
    offerToReceiveVideo: video
  }).then(offer => {
    // Guard here to ensure that the generated offer really
    // contain the number of media lines we want
    const { sdp } = offer;

    if(audio) {
      assert_equals(countAudioLine(sdp), 1,
        'Expect m=audio line to be present in generated SDP');
    } else {
      assert_equals(countAudioLine(sdp), 0,
        'Expect m=audio line to be present in generated SDP');
    }

    if(video) {
      assert_equals(countVideoLine(sdp), 1,
        'Expect m=video line to be present in generated SDP');
    } else {
      assert_equals(countVideoLine(sdp), 0,
        'Expect m=video line to not present in generated SDP');
    }

    if(data) {
      assert_equals(countApplicationLine(sdp), 1,
        'Expect m=application line to be present in generated SDP');
    } else {
      assert_equals(countApplicationLine(sdp), 0,
        'Expect m=application line to not present in generated SDP');
    }

    return offer;
  });
}

// Helper function to generate answer based on given offer using a freshly
// created RTCPeerConnection object
function generateAnswer(offer) {
  const pc = new RTCPeerConnection();
  return pc.setRemoteDescription(offer)
  .then(() => pc.createAnswer());
}

// Wait for peer connection to fire onsignalingstatechange
// event, compare and make sure the new state is the same
// as expected state. It accepts an RTCPeerConnection object
// and an array of expected state changes. The test passes
// if all expected state change events have been fired, and
// fail if the new state is different from the expected state.
//
// Note that the promise is never resolved if no change
// event is fired. To avoid confusion with the main test
// getting timed out, this is done in parallel as a separate
// test
function test_state_change_event(parentTest, pc, expectedStates) {
  return async_test(t => {
    pc.onsignalingstatechange = t.step_func(() => {
      if(expectedStates.length === 0) {
        return;
      }

      const newState = pc.signalingState;
      const expectedState = expectedStates.shift();

      assert_equals(newState, expectedState, 'New signaling state is different from expected.');

      if(expectedStates.length === 0) {
        t.done();
      }
    });
  }, `Test onsignalingstatechange event for ${parentTest.name}`);
}

// Run a test function that return a promise that should
// never be resolved. For lack of better options,
// we wait for a time out and pass the test if the
// promise doesn't resolve within that time.
function test_never_resolve(testFunc, testName) {
  async_test(t => {
    testFunc(t)
    .then(
      t.step_func(result => {
        assert_unreached(`Pending promise should never be resolved. Instead it is fulfilled with: ${result}`);
      }),
      t.step_func(err => {
        assert_unreached(`Pending promise should never be resolved. Instead it is rejected with: ${err}`);
      }));

    t.step_timeout(t.step_func_done(), 100)
  }, testName);
}
