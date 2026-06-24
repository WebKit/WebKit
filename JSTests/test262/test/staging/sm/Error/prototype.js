// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
  pending
esid: pending
includes: [nativeErrors.js]
---*/

assert.sameValue(Reflect.getPrototypeOf(Error.prototype), Object.prototype)

for (const error of nativeErrors) {
    assert.sameValue(Reflect.getPrototypeOf(error.prototype), Error.prototype);
}

