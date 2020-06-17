// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.waitasync
description: >
  Test Atomics.waitAsync on arrays that allow atomic operations
flags: [async]
includes: [atomicsHelper.js]
features: [Atomics.waitAsync, Atomics, BigInt]
---*/
assert.sameValue(typeof Atomics.waitAsync, 'function');


$262.agent.start(`
  (async () => {
    var sab = new SharedArrayBuffer(2048);
    var good_indices = [ (view) => 0/-1, // -0
                         (view) => '-0',
                         (view) => view.length - 1,
                         (view) => ({ valueOf: () => 0 }),
                         (view) => ({ toString: () => '0', valueOf: false }) // non-callable valueOf triggers invocation of toString
                       ];

    var view = new BigInt64Array(sab, 32, 20);

    view[0] = 0n;
    $262.agent.report("A " + (await Atomics.waitAsync(view, 0, 0n, 0).value))
    $262.agent.report("B " + (await Atomics.waitAsync(view, 0, 37n, 0).value));

    const results = [];
    // In-bounds boundary cases for indexing
    for ( let IdxGen of good_indices ) {
        let Idx = IdxGen(view);
        view.fill(0n);
        // Atomics.store() computes an index from Idx in the same way as other
        // Atomics operations, not quite like view[Idx].
        Atomics.store(view, Idx, 37n);
        results.push(await Atomics.waitAsync(view, Idx, 0n).value);
    }
    $262.agent.report("C " + results.join(","));

    $262.agent.leaving();
  })();
`);


Promise.all([
  $262.agent.getReportAsync(),
  $262.agent.getReportAsync(),
  $262.agent.getReportAsync(),
]).then(outcomes => {

  assert.sameValue(
    outcomes[0],
    'A timed-out',
    '"A " + (await Atomics.waitAsync(view, 0, 0n, 0).value resolves to "A timed-out"'
  );

  assert.sameValue(
    outcomes[1],
    'B not-equal',
    '"B " + (await Atomics.waitAsync(view, 0, 37n, 0).value resolves to "B not-equal"'
  );
  assert.sameValue(
    outcomes[2],
    'C not-equal,not-equal,not-equal,not-equal,not-equal',
    'All C values are not equal'
  );
}, $DONE).then($DONE, $DONE);


