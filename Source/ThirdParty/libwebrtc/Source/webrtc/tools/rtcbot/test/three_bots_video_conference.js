// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// A video conference between 3 bots streaming video and audio between
// each other.
// The test succeeds after establishing the call between the three
// devices.
//
// Note: the source of the video and audio stream is getUserMedia().
function testTwoWayVideoStreaming(test, bot1, bot2, bot3) {
  var answersCount = 0;
  var statsCollector;

  test.wait([
      createBotPeerConnectionsWithLocalStream.bind(bot1),
      createBotPeerConnectionsWithLocalStream.bind(bot2),
      createBotPeerConnectionsWithLocalStream.bind(bot3)],
    onPeerConnectionCreated);

  // done() callback is called with list of peers as argument.
  function createBotPeerConnectionsWithLocalStream(done) {
    var peerConnections = [];

    this.getUserMedia({video:true, audio:true},
        onUserMediaSuccess.bind(this), test.fail);

    function onUserMediaSuccess(stream) {
      test.log("User has granted access to local media.");
      this.showStream(stream.id, true, true);

      test.createTurnConfig(onTurnConfig.bind(this), test.fail);

      function onTurnConfig(config) {
        this.createPeerConnection(config, addStream.bind(this),
            test.fail);
        this.createPeerConnection(config, addStream.bind(this),
            test.fail);
      }

      function addStream(pc) {
        pc.addStream(stream);
        pc.addEventListener('addstream', onAddStream.bind(this));

        peerConnections.push(pc);
        if(peerConnections.length == 2)
          done(peerConnections);
      }
    }
  }

  function onPeerConnectionCreated(peerConnections1,
      peerConnections2, peerConnections3) {
    test.log("RTC Peers created.");

    // Bot1 and Bot2
    establichCall(peerConnections1[0], peerConnections2[1]);
    // Bot2 and Bot3
    establichCall(peerConnections2[0], peerConnections3[1]);
    // Bot3 and Bot1
    establichCall(peerConnections3[0], peerConnections1[1]);
  }

  function establichCall(pc1, pc2) {
    pc1.addEventListener('icecandidate', onIceCandidate.bind(pc2));
    pc2.addEventListener('icecandidate', onIceCandidate.bind(pc1));

    createOfferAndAnswer(pc1, pc2);
  }

  function onAddStream(event) {
    test.log("On Add stream.");
    this.showStream(event.stream.id, true, false);
  }

  function onIceCandidate(event) {
    if(event.candidate) {
      this.addIceCandidate(event.candidate,
         onAddIceCandidateSuccess, test.fail);
    };

    function onAddIceCandidateSuccess() {
      test.log("Candidate added successfully");
    };
  }

  function createOfferAndAnswer(pc1, pc2) {
    test.log("Creating offer.");
    pc1.createOffer(gotOffer, test.fail);

    function gotOffer(offer) {
      test.log("Got offer");
      pc1.setLocalDescription(offer, onSetSessionDescriptionSuccess, test.fail);
      pc2.setRemoteDescription(offer, onSetSessionDescriptionSuccess,
          test.fail);
      test.log("Creating answer");
      pc2.createAnswer(gotAnswer, test.fail);
    }

    function gotAnswer(answer) {
      test.log("Got answer");
      pc2.setLocalDescription(answer, onSetSessionDescriptionSuccess,
          test.fail);
      pc1.setRemoteDescription(answer, onSetSessionDescriptionSuccess,
          test.fail);

      answersCount++;
      if(answersCount == 3) {
        // SetTimeout used because creating the three answers will very fast
        // and test will success and the vm will be closed before establishing
        // the calls.
        setTimeout(function() {
            test.done();
          }, 5000);
      }
    }

    function onSetSessionDescriptionSuccess() {
      test.log("Set session description success.");
    }
  }
}

registerBotTest('threeBotsVideoConference/android+android+chrome',
                testTwoWayVideoStreaming, ['android-chrome', 'android-chrome',
                'chrome']);
registerBotTest('threeBotsVideoConference/chrome-chrome-chrome',
                testTwoWayVideoStreaming, ['chrome', 'chrome', 'chrome']);
registerBotTest('threeBotsVideoConference/android-android-android',
                testTwoWayVideoStreaming, ['android-chrome', 'android-chrome',
                'android-chrome']);