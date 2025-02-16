// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
class badBase {}
assertThrowsInstanceOf(badBase, TypeError);

class badSub extends (class {}) {}
assertThrowsInstanceOf(badSub, TypeError);

