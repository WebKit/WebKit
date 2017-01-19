// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// A unidirectional video and audio flowing test from bot 1 to bot 2,
// and download a file from a server after 2 seconds of establishing
// the call.
//
// The test succeeds after collecting stats for 10 seconds from both bots
// and then write these stats to a file.
//
// Note: the source of the video and audio stream is getUserMedia().
//
function testOneWayVideoWithDownloading(test, bot1, bot2) {
  var report = test.createStatisticsReport("testOneWayVideoWithDownloading");

  test.wait([
      createPeerConnection.bind(bot1),
      createPeerConnection.bind(bot2) ],
    onPeerConnectionCreated);

  function createPeerConnection(done) {
    test.createTurnConfig(onTurnConfig.bind(this), test.fail);

    function onTurnConfig(config) {
      this.createPeerConnection(config, done, test.fail);
    };
  }

  function onPeerConnectionCreated(pc1, pc2) {
    test.log("RTC Peers created.");
    pc1.addEventListener('addstream', test.fail);
    pc2.addEventListener('addstream', onAddStream);
    pc1.addEventListener('icecandidate', onIceCandidate.bind(pc2));
    pc2.addEventListener('icecandidate', onIceCandidate.bind(pc1));

    bot1.getUserMedia({video:true, audio:true}, onUserMediaSuccess, test.fail);

    function onUserMediaSuccess(stream) {
      test.log("User has granted access to local media.");
      pc1.addStream(stream);
      bot1.showStream(stream.id, true, true);

      createOfferAndAnswer(pc1, pc2);
    }
  }

  function onAddStream(event) {
    test.log("On Add stream.");
    bot2.showStream(event.stream.id, true, false);
  }

  function onIceCandidate(event) {
    if(event.candidate) {
      test.log(event.candidate.candidate);
      this.addIceCandidate(event.candidate,
         onAddIceCandidateSuccess, test.fail);
    }

    function onAddIceCandidateSuccess() {
      test.log("Candidate added successfully");
    }
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

      setTimeout(function() {
        downloadFile(bot1, "bot1");
        downloadFile(bot2, "bot2");
      }, 2000);
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

    function downloadFile(bot, name) {
      bot.downloadFile("https://test.webrtc.org/test-download-file/9000KB.data",
          onDownloadSuccess.bind(null, name), test.fail);

      function onDownloadSuccess(name, data) {
        test.log( name + " downloaded " +
            Math.round(data.length/(1024*1024)) + "MB.");
      }
    }
  }
}

registerBotTest('testOneWayVideoWithDownloading/chrome-chrome',
                testOneWayVideoWithDownloading, ['chrome', 'chrome']);
