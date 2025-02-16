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
if (typeof window === "undefined")
    window = this;

Object.defineProperty(window, "foo", {
    get: function() { return 5; },
    configurable: true
});

for (var i = 0; i < 100; ++i)
    assert.sameValue(window.foo, 5);

