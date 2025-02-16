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
class base { constructor() { } }

class inst extends base { constructor() { super(); } }
Object.setPrototypeOf(inst, Math.sin);
assertThrowsInstanceOf(() => new inst(), TypeError);

class defaultInst extends base { }
Object.setPrototypeOf(inst, Math.sin);
assertThrowsInstanceOf(() => new inst(), TypeError);

