"use strict"

description('This tests basic properties of the TypedArray prototype');

let arrayTypes = [
    Int8Array,
    Int16Array,
    Int32Array,
    Uint8Array,
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

// length.
shouldBeFalse('"writable" in Object.getOwnPropertyDescriptor(TypedArray.prototype, "length")');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").configurable');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.length', '0');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get', 'function');
shouldBe('Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").set', 'undefined');

// toString.
shouldBe('TypedArray.prototype.toString', 'Array.prototype.toString');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").writable');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").value', 'function');

// toLocaleString.
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toLocaleString").writable');
shouldBeFalse('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toLocaleString").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(TypedArray.prototype, "toLocaleString").configurable');
shouldBeEqualToString('typeof Object.getOwnPropertyDescriptor(TypedArray.prototype, "toString").value', 'function');
