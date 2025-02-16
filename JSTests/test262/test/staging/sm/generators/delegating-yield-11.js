// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-generators-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// The first call to yield* passes one arg to "next".

function Iter() {
    function next() {
        if (arguments.length != 1)
            throw Error;
        return { value: 42, done: true }
    }

    this.next = next;
    this[Symbol.iterator] = function () { return this; }
}

function* delegate(iter) { return yield* iter; }

var iter = delegate(new Iter());
assert.deepEqual(iter.next(), {value:42, done:true});

