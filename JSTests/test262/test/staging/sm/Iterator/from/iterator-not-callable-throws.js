// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
  Iterator.from throws when called with an object with a non-callable @@iterator property.

  Iterator is not enabled unconditionally
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
features:
  - iterator-helpers
description: |
  pending
esid: pending
---*/
assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: 0 }), TypeError);
assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: false }), TypeError);
assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: "" }), TypeError);
assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: {} }), TypeError);
assertThrowsInstanceOf(() => Iterator.from({ [Symbol.iterator]: Symbol('') }), TypeError);

