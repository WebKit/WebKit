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

// Re-entering the map() generator from the called mapper fails.

let iterator;
function mapper(x) {
  let n = iterator.next();
  return x;
}
iterator = [0].values().map(mapper);

assertThrowsInstanceOf(iterator.next, TypeError);

