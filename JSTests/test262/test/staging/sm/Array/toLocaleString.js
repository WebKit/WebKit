// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
flags:
  - onlyStrict
includes: [sm/non262.js, sm/non262-shell.js]
description: |
  pending
esid: pending
---*/
"use strict";

Object.defineProperty(String.prototype, "toLocaleString", {
    get() {
        assert.sameValue(typeof this, "string");

        return function() { return typeof this; };
    }
})

assert.sameValue(["test"].toLocaleString(), "string");

