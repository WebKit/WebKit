//@ requireOptions("--useIteratorHelpers=1")

function assert(a, b) {
    if (a !== b)
        throw new Error("Expected: " + b + " but got: " + a);
}

function assertThrows(expectedError, f) {
    try {
        f();
    } catch (e) {
        assert(e instanceof expectedError, true);
    }
}

// see https://tc39.es/proposal-iterator-helpers/#sec-set-iteratorprototype-@@tostringtag
assertThrows(TypeError, function () {
    Iterator.prototype[Symbol.toStringTag] = "foo";
});

var setter = Object.getOwnPropertyDescriptor(Iterator.prototype, Symbol.toStringTag).set;

assertThrows(TypeError, function () {
    setter.call("foo");
});

assertThrows(TypeError, function () {
    setter.call(Object.preventExtensions({}));
});

class MyError extends Error {}

assertThrows(MyError, function () {
    setter.call({ set [Symbol.toStringTag](v) { throw v; } }, new MyError);
});

var logs = [];
var loggingHandler = {
    getOwnPropertyDescriptor(target, key) {
        logs.push(`getOwnPropertyDescriptor key: ${String(key)}`);
        return Reflect.getOwnPropertyDescriptor(target, key);
    },
    set(target, key, value, receiver) {
        logs.push(`set key: ${String(key)}, value: ${value}`);
        return Reflect.set(target, key, value, receiver);
    },
    defineProperty(target, key, desc) {
        logs.push(`defineProperty key: ${String(key)}, desc: ${JSON.stringify(desc)}`);
        return Reflect.defineProperty(target, key, desc);
    },
};

var proxyWithProperty = new Proxy({ [Symbol.toStringTag]: 1 }, loggingHandler);
setter.call(proxyWithProperty, 2);
assert(logs[0], "getOwnPropertyDescriptor key: Symbol(Symbol.toStringTag)");
assert(logs[1], "set key: Symbol(Symbol.toStringTag), value: 2");
assert(logs[2], "getOwnPropertyDescriptor key: Symbol(Symbol.toStringTag)");
assert(logs[3], `defineProperty key: Symbol(Symbol.toStringTag), desc: {"value":2}`);
assert(logs.length, 4);

logs = [];
var proxyWithoutProperty = new Proxy({}, loggingHandler);
setter.call(proxyWithoutProperty, 3);
assert(logs[0], "getOwnPropertyDescriptor key: Symbol(Symbol.toStringTag)");
assert(logs[1], `defineProperty key: Symbol(Symbol.toStringTag), desc: {"value":3,"writable":true,"enumerable":true,"configurable":true}`);
assert(logs.length, 2);
