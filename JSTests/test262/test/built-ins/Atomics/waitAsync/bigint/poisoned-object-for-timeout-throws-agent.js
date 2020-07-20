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
features: [Atomics.waitAsync, SharedArrayBuffer, TypedArray, Atomics, BigInt, arrow-function, async-functions]
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
    const i64a = new BigInt64Array(sab);
    Atomics.add(i64a, ${RUNNING}, 1n);

    let status1 = '';
    let status2 = '';

    try {
      Atomics.waitAsync(i64a, 0, 0n, poisonedValueOf);
    } catch (error) {
      status1 = 'poisonedValueOf';
    }
    try {
      Atomics.waitAsync(i64a, 0, 0n, poisonedToPrimitive);
    } catch (error) {
      status2 = 'poisonedToPrimitive';
    }

    $262.agent.report(status1);
    $262.agent.report(status2);
    $262.agent.leaving();
  });
`);

const i64a = new BigInt64Array(new SharedArrayBuffer(BigInt64Array.BYTES_PER_ELEMENT * 4));

$262.agent.safeBroadcastAsync(i64a, RUNNING, 1n).then(async agentCount => {
  assert.sameValue(agentCount, 1n);

  assert.sameValue(
    await $262.agent.getReportAsync(),
    'poisonedValueOf',
    'Atomics.waitAsync(i64a, 0, 0n, poisonedValueOf) throws'
  );

  assert.sameValue(
    await $262.agent.getReportAsync(),
    'poisonedToPrimitive',
    'Atomics.waitAsync(i64a, 0, 0n, poisonedToPrimitive) throws'
  );

  assert.sameValue(Atomics.notify(i64a, 0), 0, 'Atomics.notify(i64a, 0) returns 0');
}).then($DONE, $DONE);
