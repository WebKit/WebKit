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

var F, o;

F = function () {};
F.prototype = new ArrayBuffer(1);
o = new F();
try {
    o.byteLength;
} catch (ex) {
    // o is not a platform object
    assert.sameValue(ex instanceof TypeError, true);
}

o = {};
o.__proto__ = new Int32Array(1);
try {
    o.buffer.byteLength;
} catch (ex) {
    // o is not a platform object
    assert.sameValue(ex instanceof TypeError, true);
}

F = function () {};
F.prototype = new Int32Array(1);
o = new F();
try {
    o.slice(0, 1);
    reportFailure("Expected an exception!");
} catch (ex) {
}

assert.sameValue("ok", "ok", "bug 571014");
