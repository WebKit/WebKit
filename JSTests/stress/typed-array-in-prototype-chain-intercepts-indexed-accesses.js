//@ requireOptions("--useConcurrentJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

function makeTypedArrays(contents) {
    class Sub extends Uint8Array { }
    let result = [
        new Uint8Array(contents),
        new Sub(contents),
        new (createGlobalObject().Uint8Array)(contents),
    ];
    let resizable = new Uint8Array(new ArrayBuffer(contents.length, { maxByteLength: contents.length * 2 }));
    resizable.set(contents);
    result.push(resizable);
    return result;
}

// [[Set]] of an out-of-bounds index through a TypedArray prototype returns true without defining a property on the receiver.
for (let ta of makeTypedArrays([0, 0, 0, 0])) {
    let child = Object.create(ta);
    child[10] = 99;
    shouldBe(child.hasOwnProperty(10), false);
    shouldBe(child[10], undefined);
    shouldBe(ta[10], undefined);

    shouldBe(Reflect.set(child, 11, 99), true);
    shouldBe(child.hasOwnProperty(11), false);

    (function () {
        "use strict";
        child[12] = 99;
    })();
    shouldBe(child.hasOwnProperty(12), false);

    // In-bounds index goes through OrdinarySet and defines a property on the receiver.
    child[1] = 7;
    shouldBe(child.hasOwnProperty(1), true);
    shouldBe(child[1], 7);
    shouldBe(ta[1], 0);
}

// Detached and out-of-bounds TypedArrays have no valid integer indices.
{
    let detached = new Uint8Array(4);
    transferArrayBuffer(detached.buffer);
    let child = Object.create(detached);
    child[0] = 1;
    shouldBe(child.hasOwnProperty(0), false);

    let buffer = new ArrayBuffer(4, { maxByteLength: 8 });
    let outOfBounds = new Uint8Array(buffer, 2, 2);
    buffer.resize(2);
    child = Object.create(outOfBounds);
    child[0] = 1;
    shouldBe(child.hasOwnProperty(0), false);
}

function put(object, index, value) {
    object[index] = value;
}
noInline(put);

for (let ta of makeTypedArrays([0, 0, 0, 0])) {
    for (let i = 0; i < testLoopCount; ++i) {
        let child = Object.create(ta);
        put(child, 10, i);
        shouldBe(child.hasOwnProperty(10), false);
        put(child, 1, i);
        shouldBe(child.hasOwnProperty(1), true);
        shouldBe(child[1], i);
    }
    shouldBe(ta[1], 0);
}

// Holes in an array whose prototype is a TypedArray read through to the TypedArray.
for (let ta of makeTypedArrays([10, 20, 30])) {
    let arr = [1, , 3];
    arr[100000] = 4;
    Object.setPrototypeOf(arr, ta);

    function readHole(a, i) { return a[i]; }

    shouldBe(readHole(arr, 1), 20);
    let result;
    for (let i = 0; i < testLoopCount; ++i)
        result = readHole(arr, 1);
    shouldBe(result, 20);
}

for (let ta of makeTypedArrays([10, 20, 30])) {
    let arr = [1, , 3];
    Object.setPrototypeOf(arr, ta);
    arr.constructor = Array;
    let sliced = Array.prototype.slice.call(arr);
    shouldBe(sliced.hasOwnProperty(1), true);
    shouldBe(sliced[1], 20);
}

// Arrays allocated with a TypedArray as new.target.prototype.
for (let ta of makeTypedArrays([10, 20, 30])) {
    function F() { }
    F.prototype = ta;
    let arr = Reflect.construct(Array, [3], F);
    shouldBe(arr[1], 20);
    arr[5] = 1;
    shouldBe(arr.hasOwnProperty(5), false);
    shouldBe(arr.length, 3);
}

// Subclasses of String also intercept indexed accesses.
{
    class S extends String { }
    let arr = [1, , 3];
    arr[100000] = 4;
    Object.setPrototypeOf(arr, new S("abc"));

    function readStringHole(a, i) { return a[i]; }

    shouldBe(readStringHole(arr, 1), "b");
    let result;
    for (let i = 0; i < testLoopCount; ++i)
        result = readStringHole(arr, 1);
    shouldBe(result, "b");
}

// Inserting a TypedArray into the prototype chain of an existing prototype. This makes the realm have a bad time, so it must run last.
{
    let ta = new Uint8Array([10, 20, 30]);
    let middle = { };
    let arr = [1, , 3];
    Object.setPrototypeOf(arr, middle);
    Object.setPrototypeOf(middle, ta);

    function readHoleAfterBadTime(a, i) { return a[i]; }

    shouldBe(readHoleAfterBadTime(arr, 1), 20);
    let result;
    for (let i = 0; i < testLoopCount; ++i)
        result = readHoleAfterBadTime(arr, 1);
    shouldBe(result, 20);
    arr[10] = 1;
    shouldBe(arr.hasOwnProperty(10), false);
}
