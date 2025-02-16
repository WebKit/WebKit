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
function f(foo) {
    "use strict";
    foo.bar;
}

var actual;
try {
    f();
    actual = "no error";
} catch (x) {
    actual = (x instanceof TypeError) ? "type error" : "some other error";
    actual += (/use strict/.test(x)) ? " with directive" : " without directive";
}

assert.sameValue("type error without directive", actual,
              "decompiled expressions in error messages should not include directive prologues");
