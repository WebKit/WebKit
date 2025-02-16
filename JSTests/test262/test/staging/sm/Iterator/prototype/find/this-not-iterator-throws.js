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

const fn = x => x;
assertThrowsInstanceOf(Iterator.prototype.find.bind(undefined, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.find.bind({}, fn), TypeError);
assertThrowsInstanceOf(Iterator.prototype.find.bind({next: 0}, fn), TypeError);

