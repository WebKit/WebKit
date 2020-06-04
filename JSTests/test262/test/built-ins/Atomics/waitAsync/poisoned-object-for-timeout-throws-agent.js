// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.waitasync
description: >
  False timeout arg should result in an +0 timeout
info: |
  Atomics.waitAsync( typedArray, index, value, timeout )

  1. Return DoWait(async, typedArray, index, value, timeout).

  DoWait ( mode, typedArray, index, value, timeout )

  6. Let q be ? ToNumber(timeout).

  Let primValue be ? ToPrimitive(argument, hint Number).
  Return ? ToNumber(primValue).

flags: [async]
includes: [atomicsHelper.js]
features: [Atomics.waitAsync, SharedArrayBuffer, TypedArray, Atomics, arrow-function, async-functions]
---*/
assert.sameValue(typeof Atomics.waitAsync, 'function');

const RUNNING = 1;

$262.agent.start(`
  const poisonedValueOf = {
    valueOf() {
      throw new Error('should not evaluate this code');
    }
  };

  const poisonedToPrimitive = {
    [Symbol.toPrimitive]() {
      throw new Error('passing a poisoned object using @@ToPrimitive');
    }
  };

  $262.agent.receiveBroadcast(function(sab) {
    const i32a = new Int32Array(sab);
    Atomics.add(i32a, ${RUNNING}, 1);

    let status1 = '';
    let status2 = '';

    try {
      Atomics.wait(i32a, 0, 0, poisonedValueOf);
    } catch (error) {
      status1 = 'poisonedValueOf';
    }
    try {
      Atomics.wait(i32a, 0, 0, poisonedToPrimitive);
    } catch (error) {
      status2 = 'poisonedToPrimitive';
    }

    $262.agent.report(status1);
    $262.agent.report(status2);
    $262.agent.leaving();
  });
`);

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

$262.agent.safeBroadcastAsync(i32a, RUNNING, 1).then(async (agentCount) => {

  assert.sameValue(agentCount, 1);

  assert.sameValue(
    await $262.agent.getReportAsync(),
    'poisonedValueOf',
    'Atomics.wait(i32a, 0, 0, poisonedValueOf) throws'
  );

  assert.sameValue(
    await $262.agent.getReportAsync(),
    'poisonedToPrimitive',
    'Atomics.wait(i32a, 0, 0, poisonedToPrimitive) throws'
  );

  assert.sameValue(Atomics.notify(i32a, 0), 0, 'Atomics.notify(i32a, 0) returns 0');

}).then($DONE, $DONE);

