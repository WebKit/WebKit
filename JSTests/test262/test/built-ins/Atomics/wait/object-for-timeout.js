// Copyright (C) 2018 Amal Hussein.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  False timeout arg should result in a timeout value of 1
info: |
  Atomics.wait( typedArray, index, value, timeout )

  4.Let q be ? ToNumber(timeout).
    ...
    Object
    Apply the following steps:

      Let primValue be ? ToPrimitive(argument, hint Number).
      Return ? ToNumber(primValue).
features: [ Atomics ]
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
  $262.agent.report("A " + Atomics.wait(int32Array, 0, 0, {})); // {} => NaN => Infinity
  $262.agent.leaving();
})
`);

var int32Array = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT));

$262.agent.broadcast(int32Array.buffer);

$262.agent.sleep(500); // Ample time

assert.sameValue(Atomics.wake(int32Array, 0), 1);

assert.sameValue(getReport(), "A ok");
