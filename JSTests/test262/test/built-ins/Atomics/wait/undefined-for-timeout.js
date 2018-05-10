// Copyright (C) 2018 Amal Hussein.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Undefined timeout arg should result in an infinite timeout
info: |
  Atomics.wait( typedArray, index, value, timeout )

  4.Let q be ? ToNumber(timeout).
    ...
    Undefined    Return NaN.
  5.If q is NaN, let t be +∞, else let t be max(q, 0)
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

var NUMAGENT = 2; // Total number of agents started
var WAKEUP = 0; // Index all agents are waiting on
var WAKECOUNT = 2; // Total number of agents to wake up

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
  $262.agent.report("A " + Atomics.wait(int32Array, 0, 0, undefined));  // undefined => NaN => +Infinity
  $262.agent.leaving();
})
`);

$262.agent.start(
  `
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);
  $262.agent.report("B " + Atomics.wait(int32Array, 0, 0));  // undefined timeout arg => NaN => +Infinity
  $262.agent.leaving();
})
`);

var int32Array = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT));

$262.agent.broadcast(int32Array.buffer);

$262.agent.sleep(500); // Ample time

assert.sameValue($262.agent.getReport(), null);

assert.sameValue(Atomics.wake(int32Array, WAKEUP, WAKECOUNT), WAKECOUNT);

var sortedReports = [];
for (var i = 0; i < NUMAGENT; i++) {
  sortedReports.push(getReport());
}
sortedReports.sort();

assert.sameValue(sortedReports[0], "A ok");
assert.sameValue(sortedReports[1], "B ok");
