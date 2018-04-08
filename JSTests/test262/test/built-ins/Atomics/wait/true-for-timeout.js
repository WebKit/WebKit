// Copyright (C) 2018 Amal Hussein.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  True timeout arg should result in a timeout value of 1
info: |
  Atomics.wait( typedArray, index, value, timeout )

  4.Let q be ? ToNumber(timeout).
    ...
    Boolean    If argument is true, return 1. If argument is false, return +0.
features: [ Atomics, SharedArrayBuffer, TypedArray ]
includes: [atomicsHelper.js]
---*/

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null)
    $262.agent.sleep(100);
  return r;
}


$262.agent.start(
  `
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);
  var start = Date.now();
  $262.agent.report(Atomics.wait(int32Array, 0, 0, true));  // true => 1
  $262.agent.report(Date.now() - start);
  $262.agent.leaving();
})
`);

var int32Array = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT));

$262.agent.broadcast(int32Array.buffer);

$262.agent.sleep(2);

var r1 = getReport();
var r2 = getReport();

assert.sameValue(r1, "timed-out");
assert(r2 >= 1, "timeout should be a min of 1ms");
assert(r2 <= $ATOMICS_MAX_TIME_EPSILON + 1, "timeout should be a max of $ATOMICS_MAX_TIME_EPSILON + 1ms");
