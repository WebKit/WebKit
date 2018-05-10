// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test that Atomics.wake wakes zero waiters if that's what the count is.
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

var NUMAGENT = 3;
var WAKEUP = 0;                 // Agents wait here
var RUNNING = 1;                // Accounting of live agents here
var NUMELEM = 2;

var WAKECOUNT = 0;

for ( var i=0 ; i < NUMAGENT ; i++ ) {
$262.agent.start(
`
$262.agent.receiveBroadcast(function (sab) {
  var ia = new Int32Array(sab);
  Atomics.add(ia, ${RUNNING}, 1);
  // Waiters that are not woken will time out eventually.
  $262.agent.report(Atomics.wait(ia, ${WAKEUP}, 0, 200));
  $262.agent.leaving();
})
`);
}

var ia = new Int32Array(new SharedArrayBuffer(NUMELEM * Int32Array.BYTES_PER_ELEMENT));
$262.agent.broadcast(ia.buffer);

// Wait for agents to be running.
waitUntil(ia, RUNNING, NUMAGENT);

// Then wait some more to give the agents a fair chance to wait.  If we don't,
// we risk sending the wakeup before agents are sleeping, and we hang.
$262.agent.sleep(50);

// There's a slight risk we'll fail to wake the desired count, if the preceding
// sleep() took much longer than anticipated and workers have started timing
// out.
assert.sameValue(Atomics.wake(ia, 0, WAKECOUNT), WAKECOUNT);

// Collect and check results
var rs = [];
for (var i = 0; i < NUMAGENT; i++) {
  rs.push(getReport());
}
rs.sort();

for (var i = 0; i < WAKECOUNT; i++) {
  assert.sameValue(rs[i], "ok", "The value of rs[i] is ok");
}
for (var i = WAKECOUNT; i < NUMAGENT; i++) {
  assert.sameValue(rs[i], "timed-out", "The value of rs[i] is timed-out");
}

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null)
    $262.agent.sleep(10);
  return r;
}

function waitUntil(ia, k, value) {
  var i = 0;
  while (Atomics.load(ia, k) !== value && i < 15) {
    $262.agent.sleep(10);
    i++;
  }
  assert.sameValue(Atomics.load(ia, k), value, "Atomics.load(ia, k) returns value (All agents are running)");
}
