// Copyright (C) 2018 Amal Hussein.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  New waiters should be applied to the end of the list and woken by order they entered the list (FIFO)
info: |
  Atomics.wait( typedArray, index, value, timeout )

  16.Perform AddWaiter(WL, W).
    ...
    3.Add W to the end of the list of waiters in WL.

features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

function getReport() {
  var r;
  while ((r = $262.agent.getReport()) == null) {
    $262.agent.sleep(100);
  }
  return r;
}

var agent1 = '1';
var agent2 = '2';
var agent3 = '3';

$262.agent.start(
`
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);

  $262.agent.report(${agent1});
  Atomics.wait(int32Array, 0, 0);
  $262.agent.report(${agent1});

  $262.agent.leaving();
})
`);

$262.agent.start(
  `
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);

  $262.agent.report(${agent2});

  Atomics.wait(int32Array, 0, 0);
  $262.agent.report(${agent2});

  $262.agent.leaving();
})
`);

$262.agent.start(
  `
$262.agent.receiveBroadcast(function (sab) {
  var int32Array = new Int32Array(sab);

  $262.agent.report(${agent3});

  Atomics.wait(int32Array, 0, 0);
  $262.agent.report(${agent3});

  $262.agent.leaving();
})
`);


var int32Array = new Int32Array(new SharedArrayBuffer(4));

$262.agent.broadcast(int32Array.buffer);

var orderWhichAgentsWereStarted = getReport() + getReport() + getReport(); // can be started in any order

assert.sameValue(Atomics.wake(int32Array, 0, 1), 1);

var orderAgentsWereWoken = getReport();

assert.sameValue(Atomics.wake(int32Array, 0, 1), 1);

orderAgentsWereWoken += getReport();

assert.sameValue(Atomics.wake(int32Array, 0, 1), 1);

orderAgentsWereWoken += getReport();

assert.sameValue(orderWhichAgentsWereStarted, orderAgentsWereWoken);  // agents should wake in the same order as they were started FIFO
