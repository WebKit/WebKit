//@ runDefault

// https://tc39.es/proposal-class-brand-check/#sec-runtime-semantics-classdefinitionevaluation
// The default derived constructor should not observe Array.prototype[Symbol.iterator].

// The outer for-of should call the overridden Array.prototype[Symbol.iterator].
// That function returns undefined, so for-of should throw a TypeError "Iterator result interface is not an object".
// If the default derived constructor observes Array.prototype[Symbol.iterator],
// this test recurses and throws a RangeError instead.

const originalIterator = Array.prototype[Symbol.iterator];
let ok = false;

try {
    Array.prototype[Symbol.iterator] = function () {
        class A {}
        class B extends A {}
        new B();
    };

    for (const _ of []) {}
} catch (e) {
    if (e instanceof RangeError)
        throw new Error("default derived constructor observed Array.prototype[Symbol.iterator] and threw a RangeError");
    if (e instanceof TypeError)
        ok = true;
} finally {
    Array.prototype[Symbol.iterator] = originalIterator;
}

if (!ok)
    throw new Error("for-of did not throw TypeError for invalid iterator result");
