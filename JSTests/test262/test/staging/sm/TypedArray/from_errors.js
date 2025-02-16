// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
for (var constructor of anyTypedArrayConstructors) {
    // %TypedArray%.from throws if the argument is undefined or null.
    assertThrowsInstanceOf(() => constructor.from(), TypeError);
    assertThrowsInstanceOf(() => constructor.from(undefined), TypeError);
    assertThrowsInstanceOf(() => constructor.from(null), TypeError);

    // Unlike Array.from, %TypedArray%.from doesn't get or set the length property.
    function ObjectWithThrowingLengthGetterSetter(...rest) {
        var ta = new constructor(...rest);
        Object.defineProperty(ta, "length", {
            configurable: true,
            get() { throw new RangeError("getter!"); },
            set() { throw new RangeError("setter!"); }
        });
        return ta;
    }
    ObjectWithThrowingLengthGetterSetter.from = constructor.from;
    assert.sameValue(ObjectWithThrowingLengthGetterSetter.from([123])[0], 123);

    // %TypedArray%.from throws if mapfn is neither callable nor undefined.
    assertThrowsInstanceOf(() => constructor.from([3, 4, 5], {}), TypeError);
    assertThrowsInstanceOf(() => constructor.from([3, 4, 5], "also not a function"), TypeError);
    assertThrowsInstanceOf(() => constructor.from([3, 4, 5], null), TypeError);

    // Even if the function would not have been called.
    assertThrowsInstanceOf(() => constructor.from([], JSON), TypeError);

    // If mapfn is not undefined and not callable, the error happens before anything else.
    // Before calling the constructor, before touching the arrayLike.
    var log = "";
    var obj;
    function C(...rest) {
        log += "C";
        obj = new constructor(...rest);
        return obj;
    }
    var p = new Proxy({}, {
        has: function () { log += "1"; },
        get: function () { log += "2"; },
        getOwnPropertyDescriptor: function () { log += "3"; }
    });
    assertThrowsInstanceOf(() => constructor.from.call(C, p, {}), TypeError);
    assert.sameValue(log, "");

    // If mapfn throws, the new object has already been created.
    var arrayish = {
        get length() { log += "l"; return 1; },
        get 0() { log += "0"; return "q"; }
    };
    log = "";
    var exc = {surprise: "ponies"};
    assertThrowsValue(() => constructor.from.call(C, arrayish, () => { throw exc; }), exc);
    assert.sameValue(log, "lC0");
    assert.sameValue(obj instanceof constructor, true);

    // It's a TypeError if the @@iterator property is a primitive (except null and undefined).
    for (var primitive of ["foo", 17, Symbol(), true]) {
        assertThrowsInstanceOf(() => constructor.from({[Symbol.iterator] : primitive}), TypeError);
    }
    assert.deepEqual(constructor.from({[Symbol.iterator]: null}), new constructor());
    assert.deepEqual(constructor.from({[Symbol.iterator]: undefined}), new constructor());

    // It's a TypeError if the iterator's .next() method returns a primitive.
    for (var primitive of [undefined, null, "foo", 17, Symbol(), true]) {
        assertThrowsInstanceOf(
            () => constructor.from({
                [Symbol.iterator]() {
                    return {next() { return primitive; }};
                }
            }),
        TypeError);
    }
}

