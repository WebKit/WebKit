function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function shouldThrow(fn, errorMessage)
{
    let thrown = false;
    try {
        fn();
    } catch (e) {
        thrown = true;
        if (e.message !== errorMessage)
            throw new Error(`Expected error message "${errorMessage}" but got "${e.message}"`);
    }
    if (!thrown)
        throw new Error('Expected to throw but did not');
}

// The value argument is required.
{
    shouldThrow(() => structuredClone(), 'Not enough arguments');
    shouldBe(structuredClone.length, 1);
}

// Primitives round-trip by value.
{
    shouldBe(structuredClone(undefined), undefined);
    shouldBe(structuredClone(null), null);
    shouldBe(structuredClone(true), true);
    shouldBe(structuredClone(42), 42);
    shouldBe(Object.is(structuredClone(-0), -0), true);
    shouldBe(Number.isNaN(structuredClone(NaN)), true);
    shouldBe(structuredClone('hello'), 'hello');
    shouldBe(structuredClone(123n), 123n);
}

// Objects are copied, not aliased.
{
    let source = { a: 1, nested: { b: 2 } };
    let clone = structuredClone(source);
    shouldBe(clone === source, false);
    shouldBe(clone.nested === source.nested, false);
    shouldBe(clone.a, 1);
    shouldBe(clone.nested.b, 2);

    clone.nested.b = 3;
    shouldBe(source.nested.b, 2);
}

// Cycles and shared references keep their shape.
{
    let source = {};
    source.self = source;
    let clone = structuredClone(source);
    shouldBe(clone.self === clone, true);

    let shared = { v: 1 };
    let diamond = structuredClone({ left: shared, right: shared });
    shouldBe(diamond.left === diamond.right, true);
    shouldBe(diamond.left === shared, false);
}

// Arrays keep length, holes, and named properties.
{
    let source = [1, , 3];
    source.named = 'x';
    let clone = structuredClone(source);
    shouldBe(Array.isArray(clone), true);
    shouldBe(clone.length, 3);
    shouldBe(1 in clone, false);
    shouldBe(clone[2], 3);
    shouldBe(clone.named, 'x');
}

// Date.
{
    let clone = structuredClone(new Date(1234567890));
    shouldBe(clone instanceof Date, true);
    shouldBe(clone.getTime(), 1234567890);
}

// RegExp keeps source and flags. lastIndex is not part of the serialization.
{
    let source = /ab+c/gi;
    source.lastIndex = 3;
    let clone = structuredClone(source);
    shouldBe(clone instanceof RegExp, true);
    shouldBe(clone.source, 'ab+c');
    shouldBe(clone.flags, 'gi');
    shouldBe(clone.lastIndex, 0);
}

// Map and Set, including the identity of values stored in both.
{
    let key = { k: 1 };
    let clone = structuredClone(new Map([[key, 'v'], ['s', key]]));
    shouldBe(clone instanceof Map, true);
    shouldBe(clone.size, 2);
    let clonedKey = [...clone.keys()][0];
    shouldBe(clonedKey === key, false);
    shouldBe(clonedKey.k, 1);
    shouldBe(clone.get(clonedKey), 'v');
    shouldBe(clone.get('s') === clonedKey, true);

    let set = structuredClone(new Set([1, 'two', { three: 3 }]));
    shouldBe(set instanceof Set, true);
    shouldBe(set.size, 3);
    shouldBe(set.has(1), true);
    shouldBe(set.has('two'), true);
    shouldBe([...set][2].three, 3);
}

// Errors keep their type, message, and cause.
{
    let clone = structuredClone(new TypeError('the message', { cause: 'the cause' }));
    shouldBe(clone instanceof TypeError, true);
    shouldBe(clone.name, 'TypeError');
    shouldBe(clone.message, 'the message');
    shouldBe(clone.cause, 'the cause');
    shouldBe(typeof clone.stack, 'string');
    shouldBe('cause' in structuredClone(new TypeError('no cause')), false);
}

// Boxed primitives stay boxed.
{
    shouldBe(structuredClone(new Number(5)).valueOf(), 5);
    shouldBe(structuredClone(new String('ab')).valueOf(), 'ab');
    shouldBe(structuredClone(new Boolean(true)).valueOf(), true);
    shouldBe(structuredClone(Object(123n)).valueOf(), 123n);
}

// ArrayBuffer contents are copied and the source is left attached.
{
    let source = new ArrayBuffer(8);
    new Uint8Array(source)[3] = 42;
    let clone = structuredClone(source);
    shouldBe(clone instanceof ArrayBuffer, true);
    shouldBe(clone === source, false);
    shouldBe(source.byteLength, 8);
    shouldBe(clone.byteLength, 8);
    shouldBe(new Uint8Array(clone)[3], 42);

    new Uint8Array(clone)[3] = 7;
    shouldBe(new Uint8Array(source)[3], 42);
}

// A resizable ArrayBuffer stays resizable.
{
    let clone = structuredClone(new ArrayBuffer(8, { maxByteLength: 16 }));
    shouldBe(clone.byteLength, 8);
    shouldBe(clone.maxByteLength, 16);
    shouldBe(clone.resizable, true);
}

// Views over one buffer still share one buffer after cloning.
{
    let buffer = new ArrayBuffer(8);
    let clone = structuredClone({ first: new Uint8Array(buffer), second: new Uint8Array(buffer) });
    shouldBe(clone.first.buffer === clone.second.buffer, true);
    clone.first[0] = 9;
    shouldBe(clone.second[0], 9);
    shouldBe(new Uint8Array(buffer)[0], 0);
}

// Typed arrays and DataViews keep their type and window onto the buffer.
{
    let clone = structuredClone(new Float64Array([1.5, 2.5]));
    shouldBe(clone instanceof Float64Array, true);
    shouldBe(clone.length, 2);
    shouldBe(clone[1], 2.5);

    let view = structuredClone(new DataView(new ArrayBuffer(8), 4, 2));
    shouldBe(view instanceof DataView, true);
    shouldBe(view.byteOffset, 4);
    shouldBe(view.byteLength, 2);
}

// SharedArrayBuffers are shared rather than copied.
{
    let source = new SharedArrayBuffer(8);
    let clone = structuredClone(source);
    shouldBe(clone instanceof SharedArrayBuffer, true);
    new Uint8Array(clone)[0] = 5;
    shouldBe(new Uint8Array(source)[0], 5);
}

// WebAssembly modules clone.
{
    let source = new WebAssembly.Module(new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0]));
    let clone = structuredClone(source);
    shouldBe(clone instanceof WebAssembly.Module, true);
    shouldBe(clone === source, false);
}

// The clone is a plain object: prototypes, symbol keys, non-enumerable properties, and property
// attributes are all dropped.
{
    class Point {
        constructor() { this.x = 1; }
        method() { }
    }
    let clone = structuredClone(new Point());
    shouldBe(Object.getPrototypeOf(clone), Object.prototype);
    shouldBe(clone.x, 1);
    shouldBe(clone.method, undefined);

    let source = { [Symbol('dropped')]: 1, kept: 2 };
    Object.defineProperty(source, 'hidden', { value: 3, enumerable: false });
    clone = structuredClone(source);
    shouldBe(Object.getOwnPropertySymbols(clone).length, 0);
    shouldBe(Object.keys(clone).join(), 'kept');
    shouldBe(clone.hidden, undefined);

    shouldBe(Object.isFrozen(structuredClone(Object.freeze({ a: 1 }))), false);
}

// Getters run once and their result becomes a data property.
{
    let calls = 0;
    let clone = structuredClone({ get value() { return ++calls; } });
    shouldBe(calls, 1);
    shouldBe(clone.value, 1);
    shouldBe(Object.getOwnPropertyDescriptor(clone, 'value').writable, true);
}

// A getter that throws propagates its own exception.
{
    shouldThrow(() => structuredClone({ get value() { throw new Error('from the getter'); } }), 'from the getter');
}

// Values with no serialized form are rejected.
{
    shouldThrow(() => structuredClone(function () { }), 'Value could not be cloned.');
    shouldThrow(() => structuredClone(Symbol('nope')), 'Value could not be cloned.');
    shouldThrow(() => structuredClone({ nested: { fn: () => { } } }), 'Value could not be cloned.');
    shouldThrow(() => structuredClone(new Proxy({ a: 1 }, { })), 'Value could not be cloned.');
}
