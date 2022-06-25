"use strict"

description('This tests basic properties of the TypedArray prototype');

let arrayTypes = [
    Int8Array,
    Int16Array,
    Int32Array,
    Uint8Array,
    Uint8ClampedArray,
    Uint16Array,
    Uint32Array,
    Float32Array,
    Float64Array,
];

// The prototype should be the same for every type of view.
for (let i = 0; i < arrayTypes.length; ++i) {
    for (let j = i; j < arrayTypes.length; ++j) {
        shouldBeTrue("Object.getPrototypeOf(" + arrayTypes[i].name + ") === Object.getPrototypeOf(" + arrayTypes[j].name + ")");
    }
}

let TypedArray = Object.getPrototypeOf(Int8Array);

// buffer.
shouldBeEqualToString('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.name', 'get buffer');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.length', '0');
shouldBeFalse('"writable" in Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer")');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get', 'function');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").set', 'undefined');
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.call()', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.call(undefined)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.call(null)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.call(5)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.call([])', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.call({ foo: "bar" })', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.call(new ArrayBuffer(42))', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.call(new DataView(new ArrayBuffer(8), 0, 1))', "'TypeError: Receiver should be a typed array view'");

// byteLength.
shouldBeEqualToString('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.name', 'get byteLength');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.length', '0');
shouldBeFalse('"writable" in Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength")');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get', 'function');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").set', 'undefined');
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.call()', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.call(undefined)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.call(null)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.call(5)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.call([])', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.call({ foo: "bar" })', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.call(new ArrayBuffer(42))', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.call(new DataView(new ArrayBuffer(8), 0, 1))', "'TypeError: Receiver should be a typed array view'");

// byteOffset.
shouldBeEqualToString('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.name', 'get byteOffset');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.length', '0');
shouldBeFalse('"writable" in Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset")');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get', 'function');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").set', 'undefined');
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.call()', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.call(undefined)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.call(null)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.call(5)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.call([])', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.call({ foo: "bar" })', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.call(new ArrayBuffer(42))', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.call(new DataView(new ArrayBuffer(8), 0, 1))', "'TypeError: Receiver should be a typed array view'");

// entries.
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "entries").writable');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "entries").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "entries").configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "entries").value', 'function');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "entries").set', 'undefined');
shouldThrow('TypedArray.prototype.entries.call()', "'TypeError: Receiver should be a typed array view'");
shouldThrow('TypedArray.prototype.entries.call(undefined)', "'TypeError: Receiver should be a typed array view'");
shouldThrow('TypedArray.prototype.entries.call(null)', "'TypeError: Receiver should be a typed array view'");
shouldThrow('TypedArray.prototype.entries.call(5)', "'TypeError: Receiver should be a typed array view'");
shouldThrow('TypedArray.prototype.entries.call([])', "'TypeError: Receiver should be a typed array view'");
shouldThrow('TypedArray.prototype.entries.call({ foo: "bar" })', "'TypeError: Receiver should be a typed array view'");
shouldThrow('TypedArray.prototype.entries.call(new ArrayBuffer(42))', "'TypeError: Receiver should be a typed array view'");
shouldThrow('TypedArray.prototype.entries.call(new DataView(new ArrayBuffer(8), 0, 1))', "'TypeError: Receiver should be a typed array view'");

// length.
shouldBeEqualToString('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.name', 'get length');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.length', '0');
shouldBeFalse('"writable" in Object.getOwnPropertyDescriptor(TypedArray.prototype, "length")');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get', 'function');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").set', 'undefined');
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.call()', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.call(undefined)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.call(null)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.call(5)', "'TypeError: Receiver should be a typed array view but was not an object'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.call([])', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.call({ foo: "bar" })', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.call(new ArrayBuffer(42))', "'TypeError: Receiver should be a typed array view'");
shouldThrow('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.call(new DataView(new ArrayBuffer(8), 0, 1))', "'TypeError: Receiver should be a typed array view'");

// toLocaleString.
shouldBeEqualToString('TypedArray.prototype.toLocaleString.name', 'toLocaleString');
shouldBe('TypedArray.prototype.toLocaleString.length', '0');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toLocaleString").writable');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toLocaleString").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toLocaleString").configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").value', 'function');

// toString.
shouldBeEqualToString('TypedArray.prototype.toString.name', 'toString');
shouldBe('TypedArray.prototype.toString.length', '0');
shouldBe('TypedArray.prototype.toString', 'Array.prototype.toString');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").writable');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").value', 'function');

// toStringTag
shouldBeEqualToString('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.name', 'get [Symbol.toStringTag]');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.length', '0');
shouldBeFalse('"writable" in Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag)');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get', 'function');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).set', 'undefined');
shouldBe('TypedArray.prototype[Symbol.toStringTag]', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.call()', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.call(undefined)', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.call(null)', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.call(5)', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.call([])', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.call({ foo: "bar" })', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.call(new ArrayBuffer(42))', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.call(new DataView(new ArrayBuffer(8), 0, 1))', 'undefined');
