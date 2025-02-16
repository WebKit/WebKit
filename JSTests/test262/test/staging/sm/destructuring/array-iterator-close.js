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
// Tests that IteratorClose is called in array destructuring patterns.

function test() {
    var returnCalled = 0;
    var returnCalledExpected = 0;
    var iterable = {};

    // empty [] calls IteratorClose regardless of "done" on the result.
    iterable[Symbol.iterator] = makeIterator({
        next: function() {
            return { done: true };
        },
        ret: function() {
            returnCalled++;
            return {};
        }
    });
    var [] = iterable;
    assert.sameValue(returnCalled, ++returnCalledExpected);

    iterable[Symbol.iterator] = makeIterator({
        ret: function() {
            returnCalled++;
            return {};
        }
    });
    var [] = iterable;
    assert.sameValue(returnCalled, ++returnCalledExpected);

    // Non-empty destructuring calls IteratorClose if iterator is not done by
    // the end of destructuring.
    var [a,b] = iterable;
    assert.sameValue(returnCalled, ++returnCalledExpected);
    var [c,] = iterable;
    assert.sameValue(returnCalled, ++returnCalledExpected);

    // throw in lhs ref calls IteratorClose
    function throwlhs() {
        throw "in lhs";
    }
    assertThrowsValue(function() {
        0, [...{}[throwlhs()]] = iterable;
    }, "in lhs");
    assert.sameValue(returnCalled, ++returnCalledExpected);

    // throw in lhs ref calls IteratorClose with falsy "done".
    iterable[Symbol.iterator] = makeIterator({
        next: function() {
            // "done" is undefined.
            return {};
        },
        ret: function() {
            returnCalled++;
            return {};
        }
    });
    assertThrowsValue(function() {
        0, [...{}[throwlhs()]] = iterable;
    }, "in lhs");
    assert.sameValue(returnCalled, ++returnCalledExpected);

    // throw in iter.next doesn't call IteratorClose
    iterable[Symbol.iterator] = makeIterator({
        next: function() {
            throw "in next";
        },
        ret: function() {
            returnCalled++;
            return {};
        }
    });
    assertThrowsValue(function() {
        var [d] = iterable;
    }, "in next");
    assert.sameValue(returnCalled, returnCalledExpected);

    // "return" must return an Object.
    iterable[Symbol.iterator] = makeIterator({
        ret: function() {
            returnCalled++;
            return 42;
        }
    });
    assertThrowsInstanceOf(function() {
        var [] = iterable;
    }, TypeError);
    assert.sameValue(returnCalled, ++returnCalledExpected);
}

test();

