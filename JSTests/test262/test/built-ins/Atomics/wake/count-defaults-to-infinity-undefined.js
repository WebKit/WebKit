// Copyright (C) 2018 Amal Hussein.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Undefined count arg should result in an infinite count
info: |
  Atomics.wake( typedArray, index, count )

  3.If count is undefined, let c be +∞.

features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

var NUMAGENT = 4; // Total number of agents started
var WAKEUP = 0; // Index all agents are waiting on

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null)
    $262.agent.sleep(10);
  return r;
}

$262.agent.start(`
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);
  $262.agent.report("A " + Atomics.wait(int32Array, ${WAKEUP}, 0, 50));
  $262.agent.leaving();
});
`);

$262.agent.start(`
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);
  $262.agent.report("B " + Atomics.wait(int32Array, ${WAKEUP}, 0, 50));
  $262.agent.leaving();
});
`);


$262.agent.start(`
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);
  $262.agent.report("C " + Atomics.wait(int32Array, ${WAKEUP}, 0, 50));
  $262.agent.leaving();
});
`);


$262.agent.start(`
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);
  $262.agent.report("D " + Atomics.wait(int32Array, ${WAKEUP}, 0, 50));
  $262.agent.leaving();
});
`);

var int32Array = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT));

$262.agent.broadcast(int32Array.buffer);

$262.agent.sleep(20); // half of timeout

assert.sameValue(Atomics.wake(int32Array, WAKEUP, undefined), NUMAGENT);

var sortedReports = [];
for (var i = 0; i < NUMAGENT; i++) {
  sortedReports.push(getReport());
}
sortedReports.sort();

assert.sameValue(sortedReports[0], "A ok");
assert.sameValue(sortedReports[1], "B ok");
assert.sameValue(sortedReports[2], "C ok");
assert.sameValue(sortedReports[3], "D ok");
