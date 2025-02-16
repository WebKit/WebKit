// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: pending
description: |
  TypeError not thrown when `this` is an Array.
features:
  - Symbol.iterator
  - iterator-helpers
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
---*/
//

Iterator.prototype.map.call([], x => x);

