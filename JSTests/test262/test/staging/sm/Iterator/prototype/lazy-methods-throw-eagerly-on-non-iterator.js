// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  Lazy %Iterator.prototype% methods don't throw when called with non-objects.
info: |
  Iterator Helpers proposal 1.1.1
features:
  - iterator-helpers
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
---*/

//
//
const methods = [
  iter => Iterator.prototype.map.bind(iter, x => x),
  iter => Iterator.prototype.filter.bind(iter, x => x),
  iter => Iterator.prototype.take.bind(iter, 1),
  iter => Iterator.prototype.drop.bind(iter, 0),
  iter => Iterator.prototype.flatMap.bind(iter, x => [x]),
];

for (const method of methods) {
  assertThrowsInstanceOf(method(undefined), TypeError);
  assertThrowsInstanceOf(method(null), TypeError);
  assertThrowsInstanceOf(method(0), TypeError);
  assertThrowsInstanceOf(method(false), TypeError);
  assertThrowsInstanceOf(method(''), TypeError);
  assertThrowsInstanceOf(method(Symbol('')), TypeError);

  // No error here.
  method({});
  method([]);
}

