// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// Test that offer/answer between 2 peers completes successfully.
//
// Note: This test does not performs ice candidate exchange and
// does not verifies that media can flow between the peers.
function testOfferAnswer(test, bot1, bot2) {
  test.wait( [ bot1.createPeerConnection.bind(bot1, null),
               bot2.createPeerConnection.bind(bot2, null) ],
            run);

  function run(pc1, pc2) {
    test.log("Establishing call.");
    pc1.createOffer(gotOffer);

    function gotOffer(offer) {
      test.log("Got offer");
      expectedCall();
      pc1.setLocalDescription(offer, expectedCall, test.fail);
      pc2.setRemoteDescription(offer, expectedCall, test.fail);
      pc2.createAnswer(gotAnswer, test.fail);
    }

    function gotAnswer(answer) {
      test.log("Got answer");
      expectedCall();
      pc2.setLocalDescription(answer, expectedCall, test.fail);
      pc1.setRemoteDescription(answer, expectedCall, test.fail);
    }

    // TODO(andresp): Implement utilities in test to write expectations
    // that certain methods must be called.
    var expectedCalls = 0;
    function expectedCall() {
      if (++expectedCalls == 6)
        test.done();
    }
  }
}

registerBotTest('testOfferAnswer/chrome-chrome',
                testOfferAnswer, ['chrome', 'chrome']);
