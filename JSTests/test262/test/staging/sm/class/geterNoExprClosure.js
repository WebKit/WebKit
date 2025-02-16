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
// getter/setter with expression closure is allowed only in object literal.

assertThrowsInstanceOf(() => eval(`
  class foo {
    constructor() {}

    get a() 1
  }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
  class foo {
    constructor() {}

    set a(v) 1
  }
`), SyntaxError);

