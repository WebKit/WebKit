// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Test that we can't confuse %StringIteratorPrototype% for a
// StringIterator object.
function TestStringIteratorPrototypeConfusion() {
    var iter = ""[Symbol.iterator]();
    assertThrowsInstanceOfWithMessage(
        () => iter.next.call(Object.getPrototypeOf(iter)),
        TypeError,
        "next method called on incompatible String Iterator");
}
TestStringIteratorPrototypeConfusion();

// Tests that we can use %StringIteratorPrototype%.next on a
// cross-compartment iterator.
function TestStringIteratorWrappers() {
    var iter = ""[Symbol.iterator]();
    assert.deepEqual(iter.next.call(createNewGlobal().eval('"x"[Symbol.iterator]()')),
		 { value: "x", done: false })
}
if (typeof createNewGlobal === "function") {
    TestStringIteratorWrappers();
}

