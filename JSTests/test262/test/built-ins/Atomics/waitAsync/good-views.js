// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.waitasync
description: >
  Test Atomics.waitAsync on arrays that allow atomic operations
includes: [atomicsHelper.js]
features: [Atomics.waitAsync, Atomics]
---*/
assert.sameValue(typeof Atomics.waitAsync, 'function');

$262.agent.start(`
  (async () => {
    var sab = new SharedArrayBuffer(1024);
    var ab = new ArrayBuffer(16);

    var good_indices = [ (view) => 0/-1, // -0
                         (view) => '-0',
                         (view) => view.length - 1,
                         (view) => ({ valueOf: () => 0 }),
                         (view) => ({ toString: () => '0', valueOf: false }) // non-callable valueOf triggers invocation of toString
                       ];

    var view = new Int32Array(sab, 32, 20);

    view[0] = 0;
    $262.agent.report("A " + (await Atomics.waitAsync(view, 0, 0, 0).value))
    $262.agent.report("B " + (await Atomics.waitAsync(view, 0, 37, 0).value));

    // In-bounds boundary cases for indexing
    for ( let IdxGen of good_indices ) {
        let Idx = IdxGen(view);
        view.fill(0);
        // Atomics.store() computes an index from Idx in the same way as other
        // Atomics operations, not quite like view[Idx].
        Atomics.store(view, Idx, 37);
        $262.agent.report("C " + (await Atomics.waitAsync(view, Idx, 0).value));
    }
    $262.agent.report("done");
    $262.agent.leaving();
  })();
`);

assert.sameValue(
  $262.agent.getReport(),
  'A timed-out',
  '"A " + (await Atomics.waitAsync(view, 0, 0, 0).value resolves to "A timed-out"'
);

assert.sameValue(
  $262.agent.getReport(),
  'B not-equal',
  '"B " + (await Atomics.waitAsync(view, 0, 37, 0).value resolves to "B not-equal"'
);

var r;
while ((r = $262.agent.getReport()) !== "done") {
  assert.sameValue(
    r,
    'C not-equal',
    '"C " + (await Atomics.waitAsync(view, Idx, 0).value resolves to "C not-equal"'
  );
}
