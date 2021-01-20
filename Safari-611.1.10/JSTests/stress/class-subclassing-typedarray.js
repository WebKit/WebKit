"use strict";

let typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

let subclasses = typedArrays.map(constructor => class extends constructor { });

function checkSubclass(constructor) {
    let inst = new constructor(10);
    inst[11] = 10;
    if (!(inst instanceof constructor && inst instanceof constructor.__proto__ && inst[11] === undefined))
        throw "subclass of " + constructor.__proto__ + " was incorrect";
}

function test() {
    subclasses.forEach(checkSubclass);
}

for (var i = 0; i < 10000; i++)
    test();
