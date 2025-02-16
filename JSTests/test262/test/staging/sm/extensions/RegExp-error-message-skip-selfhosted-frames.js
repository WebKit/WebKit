// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
for (let name of ["test", Symbol.match, Symbol.replace, Symbol.search]) {
    let methodName = typeof name === "symbol" ? `[${name.description}]` : name;
    assertThrowsInstanceOfWithMessage(
        () => RegExp.prototype[name].call({}),
        TypeError,
        `${methodName} method called on incompatible Object`);
}

