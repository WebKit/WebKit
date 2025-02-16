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
const nativeErrors = [
    EvalError,
    RangeError,
    ReferenceError,
    SyntaxError,
    TypeError,
    URIError
];

assert.sameValue(Reflect.getPrototypeOf(Error), Function.prototype)

for (const error of nativeErrors)
    assert.sameValue(Reflect.getPrototypeOf(error), Error);

