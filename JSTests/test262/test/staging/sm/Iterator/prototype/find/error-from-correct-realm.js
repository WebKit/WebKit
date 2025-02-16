// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
features:
  - iterator-helpers
info: |
  Iterator is not enabled unconditionally
description: |
  pending
esid: pending
---*/

const otherGlobal = createNewGlobal({newCompartment: true});
assert.sameValue(TypeError !== otherGlobal.TypeError, true);

const iter = [].values();

assertThrowsInstanceOf(() => iter.find(), TypeError);
assertThrowsInstanceOf(
  otherGlobal.Iterator.prototype.find.bind(iter),
  otherGlobal.TypeError,
  'TypeError comes from the realm of the method.',
);

