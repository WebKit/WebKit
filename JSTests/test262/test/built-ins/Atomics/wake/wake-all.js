// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test that Atomics.wake wakes all waiters if that's what the count is.
features: [Atomics]
---*/

var WAKEUP = 0;                 // Waiters on this will be woken
var DUMMY = 1;                  // Waiters on this will not be woken
var RUNNING = 2;                // Accounting of live agents
var NUMELEM = 3;

var NUMAGENT = 3;

for (var i=0; i < NUMAGENT; i++) {
$262.agent.start(
`
$262.agent.receiveBroadcast(function (sab) {
  var ia = new Int32Array(sab);
  Atomics.add(ia, ${RUNNING}, 1);
  $262.agent.report("A " + Atomics.wait(ia, ${WAKEUP}, 0));
  $262.agent.leaving();
})
`);
}

$262.agent.start(
`
$262.agent.receiveBroadcast(function (sab) {
  var ia = new Int32Array(sab);
  Atomics.add(ia, ${RUNNING}, 1);
  // This will always time out.
  $262.agent.report("B " + Atomics.wait(ia, ${DUMMY}, 0, 1000));
  $262.agent.leaving();
})
`);

var ia = new Int32Array(new SharedArrayBuffer(NUMELEM * Int32Array.BYTES_PER_ELEMENT));
$262.agent.broadcast(ia.buffer);

// Wait for agents to be running.
waitUntil(ia, RUNNING, NUMAGENT + 1);

// Then wait some more to give the agents a fair chance to wait.  If we don't,
// we risk sending the wakeup before agents are sleeping, and we hang.
$262.agent.sleep(500);

// Wake all waiting on WAKEUP, should be 3 always, they won't time out.
assert.sameValue(Atomics.wake(ia, WAKEUP), NUMAGENT);

var rs = [];
for (var i = 0; i < NUMAGENT + 1; i++)
  rs.push(getReport());
rs.sort();

for (var i = 0; i < NUMAGENT; i++)
  assert.sameValue(rs[i], "A ok");
assert.sameValue(rs[NUMAGENT], "B timed-out");

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null)
    $262.agent.sleep(100);
  return r;
}

function waitUntil(ia, k, value) {
  var i = 0;
  while (Atomics.load(ia, k) !== value && i < 15) {
    $262.agent.sleep(100);
    i++;
  }
  assert.sameValue(Atomics.load(ia, k), value, "All agents are running");
}
