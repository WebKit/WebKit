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

var arrays = [i8a, i16a, i32a, u8a, u16a, u32a];

var atomics = new Map();
var genericAtomics = new Map();
for (var a of arrays) {
    var map = new Map();
    atomics.set(a, map);
}
var count = 0;
for (var op of ["add", "and", "compareExchange", "exchange", "load", "or", "store", "sub", "xor"]) {
    var numExtraArgs;
    switch (op) {
    case "compareExchange":
        numExtraArgs = 2;
        break;
    case "load":
        numExtraArgs = 0;
        break;
    default:
        numExtraArgs = 1;
        break;
    }
    
    function str() {
        var str = "(function (array" + count + ", index";
        for (var i = 0; i < numExtraArgs; ++i)
            str += ", a" + i;
        str += ") { return Atomics." + op + "(array" + count + ", index";
        for (var i = 0; i < numExtraArgs; ++i)
            str += ", a" + i;
        str += "); })";
        count++;
        return str;
    }
    
    var f = eval(str());
    noInline(f);
    // Warm it up on crazy.
    for (var i = 0; i < 10000; ++i)
        f(arrays[i % arrays.length], 0, 0, 0);
    genericAtomics.set(op, f);
    
    for (var a of arrays) {
        var map = atomics.get(a);
        
        var f = eval(str());
        noInline(f);
        
        // Warm it up on something easy.
        for (var i = 0; i < 10000; ++i)
            f(a, 0, 0, 0);
        
        map.set(op, f);
    }
}

function runAtomic(array, index, init, name, args, expectedResult, expectedOutcome)
{
    for (var f of [{name: "specialized", func: atomics.get(array).get(name)},
                   {name: "generic", func: genericAtomics.get(name)}]) {
        array[index] = init;
        var result = f.func(array, index, ...args);
        if (result != expectedResult)
            throw new Error("Expected Atomics." + name + "(array, " + index + ", " + args.join(", ") + ") to return " + expectedResult + " but returned " + result + " for " + Object.prototype.toString.apply(array) + " and " + f.name);
        if (array[index] !== expectedOutcome)
            throw new Error("Expected Atomics." + name + "(array, " + index + ", " + args.join(", ") + ") to result in array[" + index + "] = " + expectedOutcome + " but got " + array[index] + " for " + Object.prototype.toString.apply(array) + " and " + f.name);
    }
}

for (var a of arrays) {
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

for (var bad of [void 0, null, false, true, 1, 0.5, Symbol(), {}, "hello", dv, u8ca, f32a, f64a]) {
    shouldFail(() => genericAtomics.get("add")(bad, 0, 0), TypeError);
    shouldFail(() => genericAtomics.get("and")(bad, 0, 0), TypeError);
    shouldFail(() => genericAtomics.get("compareExchange")(bad, 0, 0, 0), TypeError);
    shouldFail(() => genericAtomics.get("exchange")(bad, 0, 0), TypeError);
    shouldFail(() => genericAtomics.get("load")(bad, 0), TypeError);
    shouldFail(() => genericAtomics.get("or")(bad, 0, 0), TypeError);
    shouldFail(() => genericAtomics.get("store")(bad, 0, 0), TypeError);
    shouldFail(() => genericAtomics.get("sub")(bad, 0, 0), TypeError);
    shouldFail(() => genericAtomics.get("xor")(bad, 0, 0), TypeError);
}

for (var idx of [-1, -1000000000000, 10000, 10000000000000, "hello"]) {
    for (var a of arrays) {
        for (var m of [atomics.get(a), genericAtomics]) {
            shouldFail(() => m.get("add")(a, idx, 0), RangeError);
            shouldFail(() => m.get("and")(a, idx, 0), RangeError);
            shouldFail(() => m.get("compareExchange")(a, idx, 0, 0), RangeError);
            shouldFail(() => m.get("exchange")(a, idx, 0), RangeError);
            shouldFail(() => m.get("load")(a, idx), RangeError);
            shouldFail(() => m.get("or")(a, idx, 0), RangeError);
            shouldFail(() => m.get("store")(a, idx, 0), RangeError);
            shouldFail(() => m.get("sub")(a, idx, 0), RangeError);
            shouldFail(() => m.get("xor")(a, idx, 0), RangeError);
        }
    }
}

