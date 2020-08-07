//@ skip if $architecture == "x86" or $architecture == "arm"
function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

function test(f) {
    for (let i = 0; i < 1000; ++i) {
        f();
    }
}

let __oldDesc = null;
let __localLength;
function makeLengthWritable() {
    assert(__oldDesc === null);
    __oldDesc = Object.getOwnPropertyDescriptor(Uint8ClampedArray.prototype.__proto__, "length");
    assert(typeof __oldDesc.get === "function");
    Reflect.defineProperty(Uint8ClampedArray.prototype.__proto__, "length", {configurable:true, get() { return __localLength; }, set(x) { __localLength = x; }});
}

function restoreOldDesc() {
    assert(__oldDesc !== null);
    Reflect.defineProperty(Uint8ClampedArray.prototype.__proto__, "length", __oldDesc);
    __oldDesc = null;
}

test(function() {
    "use strict";
    let a = [];
    a.push(300);
    a.length = 4277;

    let x = new Uint8ClampedArray;
    a.__proto__ = x;
    let err = null;
    try {
        let y = Array.prototype.map.call(a, x => x);
    } catch(e) {
        err = e;
    }
    assert(!!err);
    assert(err.toString() === "TypeError: Attempting to configure non-configurable property on a typed array at index: 0");
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let x = new Uint8ClampedArray;
    a.__proto__ = x;
    let err = null;
    try {
        let y = Array.prototype.filter.call(a, x => true);
    } catch(e) {
        err = e;
    }
    assert(!err);
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let x = new Uint8ClampedArray;
    a.__proto__ = x;
    let err = null;
    let y = Array.prototype.filter.call(a, x => false);
    assert(y instanceof Uint8ClampedArray);
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let x = new Uint8ClampedArray;
    a.__proto__ = x;

    let err = null;
    try {
        let y = Array.prototype.slice.call(a, 0);
    } catch(e) {
        err = e;
    }
    assert(err.toString() === "TypeError: Attempting to configure non-configurable property on a typed array at index: 0");
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let x = new Uint8ClampedArray;
    a.__proto__ = x;

    makeLengthWritable();
    let y = Array.prototype.slice.call(a, 100);
    assert(y.length === 4277 - 100);
    assert(y.length === __localLength);
    assert(y instanceof Uint8ClampedArray);
    restoreOldDesc();
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let x = new Uint8ClampedArray;
    a.__proto__ = x;

    makeLengthWritable();
    let y = Array.prototype.splice.call(a);
    assert(y.length === __localLength);
    assert(y.length === 0);
    restoreOldDesc();
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let x = new Uint8ClampedArray;
    a.__proto__ = x;

    let err = null;
    try {
        let y = Array.prototype.splice.call(a, 0);
    } catch(e) {
        err = e;
    }
    assert(err.toString() === "TypeError: Attempting to configure non-configurable property on a typed array at index: 0");
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let x = new Uint8ClampedArray;
    a.__proto__ = x;

    makeLengthWritable();
    let y = Array.prototype.slice.call(a, 100);
    assert(y.length === 4277 - 100);
    assert(y.length === __localLength);
    assert(y instanceof Uint8ClampedArray);
    restoreOldDesc();
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let calls = 0;
    let target = {};
    a.__proto__ = {
        constructor: {
            [Symbol.species]: function(length) {
                assert(length === 4277)
                return new Proxy(target, {
                    defineProperty(...args) {
                        ++calls;
                        return Reflect.defineProperty(...args);
                    }
                });
            }
        }
    };
    let y = Array.prototype.map.call(a, x => x);
    assert(calls === 100);
    for (let i = 0; i < 100; ++i)
        assert(target[i] === i);
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let calls = 0;
    let target = {};
    a.__proto__ = {
        constructor: {
            [Symbol.species]: function(length) {
                assert(length === 0)
                return new Proxy(target, {
                    defineProperty(...args) {
                        ++calls;
                        return Reflect.defineProperty(...args);
                    }
                });
            }
        }
    };
    let y = Array.prototype.filter.call(a, x => true);
    assert(calls === 100);
    for (let i = 0; i < 100; ++i)
        assert(target[i] === i);
});

test(function() {
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
    }
    a.length = 4277;
    let calls = 0;
    let target = {};
    let keys = [];
    a.__proto__ = {
        constructor: {
            [Symbol.species]: function(length) {
                assert(length === 4277)
                return new Proxy(target, {
                    defineProperty(...args) {
                        keys.push(args[1])
                        ++calls;
                        return Reflect.defineProperty(...args);
                    }
                });
            }
        }
    };
    let y = Array.prototype.slice.call(a, 0);
    assert(calls === 101); // length gets defined too.
    assert(keys.length === 101);
    for (let i = 0; i < 100; ++i) {
        assert(parseInt(keys[i]) === i);
        assert(target[i] === i);
    }
    assert(keys[keys.length - 1] === "length");
    assert(target.length === 4277);
});
