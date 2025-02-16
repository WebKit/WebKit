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
var BUGNUMBER = 1180306;
var summary = 'Array.from should close iterator on error';

print(BUGNUMBER + ": " + summary);

function test(ctor, { mapVal=undefined,
                      nextVal=undefined,
                      nextThrowVal=undefined,
                      modifier=undefined,
                      exceptionVal=undefined,
                      exceptionType=undefined,
                      closed=true }) {
    let iterable = {
        closed: false,
        [Symbol.iterator]() {
            let iterator = {
                first: true,
                next() {
                    if (this.first) {
                        this.first = false;
                        if (nextThrowVal)
                            throw nextThrowVal;
                        return nextVal;
                    }
                    return { value: undefined, done: true };
                },
                return() {
                    iterable.closed = true;
                    return {};
                }
            };
            if (modifier)
                modifier(iterator, iterable);

            return iterator;
        }
    };
    if (exceptionVal) {
        let caught = false;
        try {
            ctor.from(iterable, mapVal);
        } catch (e) {
            assert.sameValue(e, exceptionVal);
            caught = true;
        }
        assert.sameValue(caught, true);
    } else if (exceptionType) {
        assertThrowsInstanceOf(() => ctor.from(iterable, mapVal), exceptionType);
    } else {
        ctor.from(iterable, mapVal);
    }
    assert.sameValue(iterable.closed, closed);
}

// == Error cases with close ==

// ES 2017 draft 22.1.2.1 step 5.e.i.1.
// Cannot test.

// ES 2017 draft 22.1.2.1 step 5.e.vi.2.
test(Array, {
    mapVal: () => { throw "map throws"; },
    nextVal: { value: 1, done: false },
    exceptionVal: "map throws",
    closed: true,
});

// ES 2017 draft 22.1.2.1 step 5.e.ix.
class MyArray extends Array {
    constructor() {
        return new Proxy({}, {
            defineProperty() {
                throw "defineProperty throws";
            }
        });
    }
}
test(MyArray, {
    nextVal: { value: 1, done: false },
    exceptionVal: "defineProperty throws",
    closed: true,
});

// ES 2021 draft 7.4.6 step 5.
// if GetMethod fails, the thrown value should be ignored.
test(MyArray, {
    nextVal: { value: 1, done: false },
    modifier: (iterator, iterable) => {
        Object.defineProperty(iterator, "return", {
            get: function() {
                iterable.closed = true;
                throw "return getter throws";
            }
        });
    },
    exceptionVal: "defineProperty throws",
    closed: true,
});
test(MyArray, {
    nextVal: { value: 1, done: false },
    modifier: (iterator, iterable) => {
        Object.defineProperty(iterator, "return", {
            get: function() {
                iterable.closed = true;
                return "non object";
            }
        });
    },
    exceptionVal: "defineProperty throws",
    closed: true,
});
test(MyArray, {
    nextVal: { value: 1, done: false },
    modifier: (iterator, iterable) => {
        Object.defineProperty(iterator, "return", {
            get: function() {
                iterable.closed = true;
                // Non callable.
                return {};
            }
        });
    },
    exceptionVal: "defineProperty throws",
    closed: true,
});

// ES 2017 draft 7.4.6 steps 6.
// if return method throws, the thrown value should be ignored.
test(MyArray, {
    nextVal: { value: 1, done: false },
    modifier: (iterator, iterable) => {
        iterator.return = function() {
            iterable.closed = true;
            throw "return throws";
        };
    },
    exceptionVal: "defineProperty throws",
    closed: true,
});

test(MyArray, {
    nextVal: { value: 1, done: false },
    modifier: (iterator, iterable) => {
        iterator.return = function() {
            iterable.closed = true;
            return "non object";
        };
    },
    exceptionVal: "defineProperty throws",
    closed: true,
});

// == Error cases without close ==

// ES 2017 draft 22.1.2.1 step 5.e.iii.
test(Array, {
    nextThrowVal: "next throws",
    exceptionVal: "next throws",
    closed: false,
});

test(Array, {
    nextVal: { value: {}, get done() { throw "done getter throws"; } },
    exceptionVal: "done getter throws",
    closed: false,
});

// ES 2017 draft 22.1.2.1 step 5.e.v.
test(Array, {
    nextVal: { get value() { throw "value getter throws"; }, done: false },
    exceptionVal: "value getter throws",
    closed: false,
});

// == Successful cases ==

test(Array, {
    nextVal: { value: 1, done: false },
    closed: false,
});

