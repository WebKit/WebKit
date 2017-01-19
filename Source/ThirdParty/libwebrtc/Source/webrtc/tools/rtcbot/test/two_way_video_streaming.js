// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// A two way video and audio flowing test between bot 1 and bot 2.
// The test succeeds after collecting stats for 10 seconds from both bots
// and then write these stats to a file.
//
// Note: the source of the video and audio stream is getUserMedia().
function testTwoWayVideoStreaming(test, bot1, bot2) {
  var report = test.createStatisticsReport("two_way_video_streaming");
  var statsCollector;

  test.wait([
      createPeerConnectionWithLocalStream.bind(bot1),
      createPeerConnectionWithLocalStream.bind(bot2)],
    onPeerConnectionCreated);

  function createPeerConnectionWithLocalStream(done) {
    this.getUserMedia({video:true, audio:true},
        onUserMediaSuccess.bind(this), test.fail);

    function onUserMediaSuccess(stream) {
      test.log("User has granted access to local media.");
      test.createTurnConfig(onTurnConfig.bind(this), test.fail);

      function onTurnConfig(config) {
        this.createPeerConnection(config, addAndShowStream.bind(this),
            test.fail);
      };

      function addAndShowStream(pc) {
        pc.addStream(stream);
        this.showStream(stream.id, true, true);

        done(pc);
      }
    }
  }

  function onPeerConnectionCreated(pc1, pc2) {
    test.log("RTC Peers created.");
    pc1.addEventListener('addstream', onAddStream.bind(bot1));
    pc2.addEventListener('addstream', onAddStream.bind(bot2));
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
      test.log(event.candidate.candidate);
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
      collectStats();
    }

    function onSetSessionDescriptionSuccess() {
      test.log("Set session description success.");
    }

    function collectStats() {
      report.collectStatsFromPeerConnection("bot1", pc1);
      report.collectStatsFromPeerConnection("bot2", pc2);

      setTimeout(function() {
        report.finish(test.done);
        }, 10000);
    }
  }
}

registerBotTest('testTwoWayVideo/android-android',
                testTwoWayVideoStreaming, ['android-chrome', 'android-chrome']);
registerBotTest('testTwoWayVideo/chrome-chrome',
                testTwoWayVideoStreaming, ['chrome', 'chrome']);