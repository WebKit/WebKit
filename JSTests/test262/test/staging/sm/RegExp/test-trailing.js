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
var BUGNUMBER = 1304737;
var summary = "Trailing .* should not be ignored on matchOnly match.";

print(BUGNUMBER + ": " + summary);

function test(r, lastIndexIsZero) {
    r.lastIndex = 0;
    r.test("foo");
    assert.sameValue(r.lastIndex, lastIndexIsZero ? 0 : 3);

    r.lastIndex = 0;
    r.test("foo\nbar");
    assert.sameValue(r.lastIndex, lastIndexIsZero ? 0 : 3);

    var input = "foo" + ".bar".repeat(20000);
    r.lastIndex = 0;
    r.test(input);
    assert.sameValue(r.lastIndex, lastIndexIsZero ? 0 : input.length);

    r.lastIndex = 0;
    r.test(input + "\nbaz");
    assert.sameValue(r.lastIndex, lastIndexIsZero ? 0 : input.length);
}

test(/f.*/, true);
test(/f.*/g, false);
test(/f.*/y, false);
test(/f.*/gy, false);

