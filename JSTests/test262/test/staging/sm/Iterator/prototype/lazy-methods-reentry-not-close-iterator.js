// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
features:
  - iterator-helpers
description: |
  pending
esid: pending
---*/

const methods = [
  [iter => iter.map, x => x],
  [iter => iter.filter, x => true],
  [iter => iter.flatMap, x => [x]],
];

for (const method of methods) {
  const iter = [1, 2, 3].values();
  const iterMethod = method[0](iter);
  let iterHelper;
  let reentered = false;
  iterHelper = iterMethod.call(iter, x => {
    if (x == 2) {
      // Reenter the currently running generator.
      reentered = true;
      assertThrowsInstanceOf(() => iterHelper.next(), TypeError);
    }
    return method[1](x);
  });

  let result = iterHelper.next();
  assert.sameValue(result.value, 1);
  assert.sameValue(result.done, false);

  assert.sameValue(reentered, false);
  result = iterHelper.next();
  assert.sameValue(reentered, true);
  assert.sameValue(result.value, 2);
  assert.sameValue(result.done, false);

  result = iterHelper.next();
  assert.sameValue(result.value, 3);
  assert.sameValue(result.done, false);

  result = iterHelper.next();
  assert.sameValue(result.value, undefined);
  assert.sameValue(result.done, true);
}

