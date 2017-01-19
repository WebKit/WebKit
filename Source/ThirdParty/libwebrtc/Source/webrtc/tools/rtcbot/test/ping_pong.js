// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
function testPingPong(test, bot) {
  test.assert(typeof bot.ping === 'function', 'Bot does not exposes ping.');

  bot.ping(gotAnswer);

  function gotAnswer(answer) {
    test.log('bot > ' + answer);
    test.done();
  }
}

registerBotTest('testPingPong/chrome', testPingPong, ['chrome']);
