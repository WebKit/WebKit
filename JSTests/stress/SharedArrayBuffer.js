// This is a basic test of SharedArrayBuffer API as we understand it.

if (SharedArrayBuffer == ArrayBuffer)
    throw new Error("SharedArrayBuffer and ArrayBuffer should be distinct");

if (SharedArrayBuffer.prototype == ArrayBuffer.prototype)
    throw new Error("SharedArrayBuffer.prototype and ArrayBuffer.prototype should be distinct");

if (SharedArrayBuffer.prototype.__proto__ != Object.prototype)
    throw new Error("SharedArrayBuffer.prototype.__proto__ should be Object.prototype");

if (!(new SharedArrayBuffer(100) instanceof SharedArrayBuffer))
    throw new Error("SharedArrayBuffer should be an instance of SharedArrayBuffer");

if (!(new ArrayBuffer(100) instanceof ArrayBuffer))
    throw new Error("ArrayBuffer should be an instance of ArrayBuffer");

if (new SharedArrayBuffer(100) instanceof ArrayBuffer)
    throw new Error("SharedArrayBuffer should not be an instance of ArrayBuffer");

if (new ArrayBuffer(100) instanceof SharedArrayBuffer)
    throw new Error("ArrayBuffer should not be an instance of SharedArrayBuffer");

function checkAtomics(name, count)
{
    if (!Atomics[name])
        throw new Error("Missing Atomics." + name);
    if (Atomics[name].length != count)
        throw new Error("Atomics." + name + " should have length " + count + " but has length " + Atomics[name].length);
}
checkAtomics("add", 3);
checkAtomics("and", 3);
checkAtomics("compareExchange", 4);
checkAtomics("exchange", 3);
checkAtomics("isLockFree", 1);
checkAtomics("load", 2);
checkAtomics("notify", 3);
checkAtomics("or", 3);
checkAtomics("store", 3);
checkAtomics("sub", 3);
checkAtomics("wait", 4);
checkAtomics("xor", 3);

// These should all succeed.
var dv = new DataView(new SharedArrayBuffer(128));
var i8a = new Int8Array(new SharedArrayBuffer(128));
var i16a = new Int16Array(new SharedArrayBuffer(128));
var i32a = new Int32Array(new SharedArrayBuffer(128));
var u8a = new Uint8Array(new SharedArrayBuffer(128));
var u8ca = new Uint8ClampedArray(new SharedArrayBuffer(128));
var u16a = new Uint16Array(new SharedArrayBuffer(128));
var u32a = new Uint32Array(new SharedArrayBuffer(128));
var f32a = new Float32Array(new SharedArrayBuffer(128));
var f64a = new Float64Array(new SharedArrayBuffer(128));

function shouldFail(f, name)
{
    try {
        f();
    } catch (e) {
        if (e.name == name.name)
            return;
        throw new Error(f + " threw the wrong error: " + e);
    }
    throw new Error(f + " succeeded!");
}

function shouldSucceed(f)
{
    f();
}

for (bad of [void 0, null, false, true, 1, 0.5, Symbol(), {}, "hello", dv, u8ca, f32a, f64a]) {
    shouldFail(() => Atomics.add(bad, 0, 0), TypeError);
    shouldFail(() => Atomics.and(bad, 0, 0), TypeError);
    shouldFail(() => Atomics.compareExchange(bad, 0, 0, 0), TypeError);
    shouldFail(() => Atomics.exchange(bad, 0, 0), TypeError);
    shouldFail(() => Atomics.load(bad, 0), TypeError);
    shouldFail(() => Atomics.or(bad, 0, 0), TypeError);
    shouldFail(() => Atomics.store(bad, 0, 0), TypeError);
    shouldFail(() => Atomics.sub(bad, 0, 0), TypeError);
    shouldFail(() => Atomics.xor(bad, 0, 0), TypeError);
}

for (bad of [void 0, null, false, true, 1, 0.5, Symbol(), {}, "hello", dv, i8a, i16a, u8a, u8ca, u16a, u32a, f32a, f64a]) {
    shouldFail(() => Atomics.notify(bad, 0, 0), TypeError);
    shouldFail(() => Atomics.wait(bad, 0, 0), TypeError);
    shouldFail(() => Atomics.waitAsync(bad, 0, 0), TypeError);
    shouldFail(() => waiterListSize(bad, 0), TypeError);
}

for (idx of [-1, -1000000000000, 10000, 10000000000000]) {
    for (a of [i8a, i16a, i32a, u8a, u16a, u32a]) {
        shouldFail(() => Atomics.add(a, idx, 0), RangeError);
        shouldFail(() => Atomics.and(a, idx, 0), RangeError);
        shouldFail(() => Atomics.compareExchange(a, idx, 0, 0), RangeError);
        shouldFail(() => Atomics.exchange(a, idx, 0), RangeError);
        shouldFail(() => Atomics.load(a, idx), RangeError);
        shouldFail(() => Atomics.or(a, idx, 0), RangeError);
        shouldFail(() => Atomics.store(a, idx, 0), RangeError);
        shouldFail(() => Atomics.sub(a, idx, 0), RangeError);
        shouldFail(() => Atomics.xor(a, idx, 0), RangeError);
    }
    shouldFail(() => Atomics.notify(i32a, idx, 0), RangeError);
    shouldFail(() => Atomics.wait(i32a, idx, 0), RangeError);
    shouldFail(() => Atomics.waitAsync(i32a, idx, 0), RangeError);
    shouldFail(() => waiterListSize(i32a, idx), RangeError);
}

for (idx of ["hello"]) {
    for (a of [i8a, i16a, i32a, u8a, u16a, u32a]) {
        shouldSucceed(() => Atomics.add(a, idx, 0));
        shouldSucceed(() => Atomics.and(a, idx, 0));
        shouldSucceed(() => Atomics.compareExchange(a, idx, 0, 0));
        shouldSucceed(() => Atomics.exchange(a, idx, 0));
        shouldSucceed(() => Atomics.load(a, idx));
        shouldSucceed(() => Atomics.or(a, idx, 0));
        shouldSucceed(() => Atomics.store(a, idx, 0));
        shouldSucceed(() => Atomics.sub(a, idx, 0));
        shouldSucceed(() => Atomics.xor(a, idx, 0));
    }
    shouldSucceed(() => Atomics.notify(i32a, idx, 0));
    shouldSucceed(() => Atomics.wait(i32a, idx, 0, 1));
    shouldSucceed(() => Atomics.waitAsync(i32a, idx, 0, 1));
}

function runAtomic(array, index, init, name, args, expectedResult, expectedOutcome)
{
    array[index] = init;
    var result = Atomics[name](array, index, ...args);
    if (result != expectedResult)
        throw new Error("Expected Atomics." + name + "(array, " + index + ", " + args.join(", ") + ") to return " + expectedResult + " but returned " + result + " for " + Object.prototype.toString.apply(array));
    if (array[index] !== expectedOutcome)
        throw new Error("Expected Atomics." + name + "(array, " + index + ", " + args.join(", ") + ") to result in array[" + index + "] = " + expectedOutcome + " but got " + array[index] + " for " + Object.prototype.toString.apply(array));
}

for (a of [i8a, i16a, i32a, u8a, u16a, u32a]) {
    runAtomic(a, 0, 13, "add", [42], 13, 55);
    runAtomic(a, 0, 13, "and", [42], 13, 8);
    runAtomic(a, 0, 13, "compareExchange", [25, 42], 13, 13);
    runAtomic(a, 0, 13, "compareExchange", [13, 42], 13, 42);
    runAtomic(a, 0, 13, "exchange", [42], 13, 42);
    runAtomic(a, 0, 13, "load", [], 13, 13);
    runAtomic(a, 0, 13, "or", [42], 13, 47);
    runAtomic(a, 0, 13, "store", [42], 42, 42);
    runAtomic(a, 0, 42, "sub", [13], 42, 29);
    runAtomic(a, 0, 13, "xor", [42], 13, 39);
}

i32a[0] = 0;
var result = Atomics.wait(i32a, 0, 1);
if (result != "not-equal")
    throw "Error: bad result from Atomics.wait: " + result;

var result = Atomics.waitAsync(i32a, 0, 1);
if (result.value != "not-equal")
    throw "Error: bad result from Atomics.waitAsync: " + result;

for (timeout of [0, 1, 10]) {
    var result = Atomics.wait(i32a, 0, 0, timeout);
    if (result != "timed-out")
        throw "Error: bad result from Atomics.wait: " + result;
}

var result = Atomics.waitAsync(i32a, 0, 0, 0);
if (result.value != "timed-out")
    throw "Error: bad result from Atomics.waitAsync: " + result;
