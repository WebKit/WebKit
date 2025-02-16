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
var BUGNUMBER = 1168416;
var summary = "Regexp.prototype.test/exec shouldn't change lastIndex if not writable.";

print(BUGNUMBER + ": " + summary);

var regex = /0/g;
Object.freeze(regex);
var str = "abc000";

var desc = Object.getOwnPropertyDescriptor(regex, "lastIndex");
assert.sameValue(desc.writable, false);
assert.sameValue(desc.value, 0);

assertThrowsInstanceOf(() => regex.test(str), TypeError);

desc = Object.getOwnPropertyDescriptor(regex, "lastIndex");
assert.sameValue(desc.writable, false);
assert.sameValue(desc.value, 0);

assertThrowsInstanceOf(() => regex.exec(str), TypeError);

desc = Object.getOwnPropertyDescriptor(regex, "lastIndex");
assert.sameValue(desc.writable, false);
assert.sameValue(desc.value, 0);

