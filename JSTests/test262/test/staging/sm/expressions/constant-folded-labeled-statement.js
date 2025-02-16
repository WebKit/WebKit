// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1499448;
var summary = "Constant folder should fold labeled statements";

print(BUGNUMBER + ": " + summary);

if (typeof disassemble === "function") {
    var code = disassemble(() => { x: 2+2; });

    if (typeof assert.sameValue === "function")
        assert.sameValue(true, /Int8 4/.test(code));
}

