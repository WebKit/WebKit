// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-generators-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Tests that the "return" method on iterators is called in yield*
// expressions.

function test() {
    var returnCalled = 0;
    var returnCalledExpected = 0;
    var nextCalled = 0;
    var nextCalledExpected = 0;
    var throwCalled = 0;
    var throwCalledExpected = 0;
    var iterable = {};
    iterable[Symbol.iterator] = makeIterator({
        next: function() {
            nextCalled++;
            return { done: false };
        },
        ret: function() {
            returnCalled++;
            return { done: true, value: "iter.return" };
        }
    });

    function* y() {
        yield* iterable;
    }

    // G.p.throw on an iterator without "throw" calls IteratorClose.
    var g1 = y();
    g1.next();
    assertThrowsInstanceOf(function() {
        g1.throw("foo");
    }, TypeError);
    assert.sameValue(returnCalled, ++returnCalledExpected);
    assert.sameValue(nextCalled, ++nextCalledExpected);
    g1.next();
    assert.sameValue(nextCalled, nextCalledExpected);

    // G.p.return calls "return", and if the result.done is true, return the
    // result.
    var g2 = y();
    g2.next();
    var v2 = g2.return("test return");
    assert.sameValue(v2.done, true);
    assert.sameValue(v2.value, "iter.return");
    assert.sameValue(returnCalled, ++returnCalledExpected);
    assert.sameValue(nextCalled, ++nextCalledExpected);
    g2.next();
    assert.sameValue(nextCalled, nextCalledExpected);

    // G.p.return calls "return", and if the result.done is false, continue
    // yielding.
    iterable[Symbol.iterator] = makeIterator({
        next: function() {
            nextCalled++;
            return { done: false };
        },
        ret: function() {
            returnCalled++;
            return { done: false, value: "iter.return" };
        }
    });
    var g3 = y();
    g3.next();
    var v3 = g3.return("test return");
    assert.sameValue(v3.done, false);
    assert.sameValue(v3.value, "iter.return");
    assert.sameValue(returnCalled, ++returnCalledExpected);
    assert.sameValue(nextCalled, ++nextCalledExpected);
    g3.next();
    assert.sameValue(nextCalled, ++nextCalledExpected);

    // G.p.return throwing does not re-call iter.return.
    iterable[Symbol.iterator] = makeIterator({
        ret: function() {
            returnCalled++;
            throw "in iter.return";
        }
    });
    var g4 = y();
    g4.next();
    assertThrowsValue(function() {
        g4.return("in test");
    }, "in iter.return");
    assert.sameValue(returnCalled, ++returnCalledExpected);

    // G.p.return expects iter.return to return an Object.
    iterable[Symbol.iterator] = makeIterator({
        ret: function() {
            returnCalled++;
            return 42;
        }
    });
    var g5 = y();
    g5.next();
    assertThrowsInstanceOf(function() {
        g5.return("foo");
    }, TypeError);
    assert.sameValue(returnCalled, ++returnCalledExpected);

    // IteratorClose expects iter.return to return an Object.
    var g6 = y();
    g6.next();
    assertThrowsInstanceOfWithMessageContains(
        () => g6.throw("foo"),
        TypeError,
        "non-object"
    );
    assert.sameValue(returnCalled, ++returnCalledExpected);

    // G.p.return passes its argument to "return".
    iterable[Symbol.iterator] = makeIterator({
        ret: function(x) {
            assert.sameValue(x, "in test");
            returnCalled++;
            return { done: true };
        }
    });
    var g7 = y();
    g7.next();
    g7.return("in test");
    assert.sameValue(returnCalled, ++returnCalledExpected);

    // If a throw method is present, do not call "return".
    iterable[Symbol.iterator] = makeIterator({
        throw: function(e) {
            throwCalled++;
            throw e;
        },
        ret: function(x) {
            returnCalled++;
            return { done: true };
        }
    });
    var g8 = y();
    g8.next();
    assertThrowsValue(function() {
        g8.throw("foo");
    }, "foo");
    assert.sameValue(throwCalled, ++throwCalledExpected);
    assert.sameValue(returnCalled, returnCalledExpected);
}

test();

