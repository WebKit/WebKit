// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Throws a TypeError if index arg can not be converted to an Integer
info: |
  Atomics.wait( typedArray, index, value, timeout )

  4. Let q be ? ToNumber(timeout).

    Symbol --> Throw a TypeError exception.

features: [Atomics, SharedArrayBuffer, Symbol, Symbol.toPrimitive, TypedArray]
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
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);
  var start = Date.now();
  try {
    Atomics.wait(int32Array, 0, 0, Symbol("1"));
  } catch (error) {
    $262.agent.report('Symbol("1")');
  }
  try {
    Atomics.wait(int32Array, 0, 0, Symbol("2"));
  } catch (error) {
    $262.agent.report('Symbol("2")');
  }
  $262.agent.report(Date.now() - start);
  $262.agent.leaving();
});
`);

var int32Array = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT));

$262.agent.broadcast(int32Array.buffer);
$262.agent.sleep(150);

assert.sameValue(getReport(), 'Symbol("1")');
assert.sameValue(getReport(), 'Symbol("2")');

var timeDiffReport = getReport();

assert(timeDiffReport >= 0, "timeout should be a min of 0ms");
assert(timeDiffReport <= $ATOMICS_MAX_TIME_EPSILON, "timeout should be a max of $$ATOMICS_MAX_TIME_EPSILON");

assert.sameValue(Atomics.wake(int32Array, 0), 0);
