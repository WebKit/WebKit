// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1108467;
var summary = "Implement RegExp.prototype.flags";

print(BUGNUMBER + ": " + summary);

assert.sameValue(RegExp.prototype.flags, "");
assert.sameValue(/foo/iymg.flags, "gimy");
assert.sameValue(RegExp("").flags, "");
assert.sameValue(RegExp("", "mygi").flags, "gimy");
assert.sameValue(RegExp("", "mygui").flags, "gimuy");
assert.sameValue(genericFlags({}), "");
assert.sameValue(genericFlags({ignoreCase: true}), "i");
assert.sameValue(genericFlags({sticky:1, unicode:1, global: 0}), "uy");
assert.sameValue(genericFlags({__proto__: {multiline: true}}), "m");
assert.sameValue(genericFlags(new Proxy({}, {get(){return true}})), "dgimsuvy");

assertThrowsInstanceOf(() => genericFlags(), TypeError);
assertThrowsInstanceOf(() => genericFlags(1), TypeError);
assertThrowsInstanceOf(() => genericFlags(""), TypeError);

function genericFlags(obj) {
    return Object.getOwnPropertyDescriptor(RegExp.prototype,"flags").get.call(obj);
}

