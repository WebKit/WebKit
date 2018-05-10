// Copyright (C) 2018 Amal Hussein.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  False timeout arg should result in an +0 timeout
info: |
  Atomics.wait( typedArray, index, value, timeout )

  4. Let q be ? ToNumber(timeout).

    Null -> Return +0.

features: [Atomics, SharedArrayBuffer, TypedArray]
includes: [ atomicsHelper.js ]
---*/

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null) {
    $262.agent.sleep(100);
  }
  return r;
}

$262.agent.start(
  `
var poisonedValueOf = {
  valueOf: function() {
    throw new Error("should not evaluate this code");
  }
};

var poisonedToPrimitive = {
  [Symbol.toPrimitive]: function() {
    throw new Error("passing a poisoned object using @@ToPrimitive");
  }
};

$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);
  var start = Date.now();
  try {
    Atomics.wait(int32Array, 0, 0, poisonedValueOf);
  } catch (error) {
    $262.agent.report("poisonedValueOf");
  }
  try {
    Atomics.wait(int32Array, 0, 0, poisonedToPrimitive);
  } catch (error) {
    $262.agent.report("poisonedToPrimitive");
  }
  $262.agent.report(Date.now() - start);
  $262.agent.leaving();
});
`);

var int32Array = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT));

$262.agent.broadcast(int32Array.buffer);
$262.agent.sleep(150);

assert.sameValue(getReport(), "poisonedValueOf");
assert.sameValue(getReport(), "poisonedToPrimitive");

var timeDiffReport = getReport();

assert(timeDiffReport >= 0, "timeout should be a min of 0ms");

assert(timeDiffReport <= $ATOMICS_MAX_TIME_EPSILON, "timeout should be a max of $$ATOMICS_MAX_TIME_EPSILON");

assert.sameValue(Atomics.wake(int32Array, 0), 0);

