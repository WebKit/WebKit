var errors = "";
var numTests = 0;

function test(type) {
    var didThrow = false;
    try {
        var bad = type(10);
    } catch(e) {
        didThrow = true;
    }

    if (!didThrow) {
        errors += ("bad result: calling " + type.name + " as a function did not throw\n");
    }
    numTests++;

    if (typeof type !== "function")
        errors += ("bad result: typeof " + type.name + " is not function. Was " + (typeof type) + "\n");
    numTests++;
}

// According to the spec, the constructors of the following types "are not intended to be
// called as a function and will throw an exception". However, as constructors, their
// type should be "function". 

// https://tc39.github.io/ecma262/#sec-typedarray-constructors
test(Int8Array);
test(Uint8Array);
test(Uint8ClampedArray);
test(Int16Array);
test(Uint16Array);
test(Int32Array);
test(Uint32Array);
test(Float32Array);
test(Float64Array);

// https://tc39.github.io/ecma262/#sec-map-constructor
test(Map);
// https://tc39.github.io/ecma262/#sec-set-constructor
test(Set);
// https://tc39.github.io/ecma262/#sec-weakmap-constructor
test(WeakMap);
// https://tc39.github.io/ecma262/#sec-weakset-constructor
test(WeakSet);
// https://tc39.github.io/ecma262/#sec-arraybuffer-constructor
test(ArrayBuffer);
// https://tc39.github.io/ecma262/#sec-dataview-constructor
test(DataView);
// https://tc39.github.io/ecma262/#sec-promise-constructor
test(Promise);
// https://tc39.github.io/ecma262/#sec-proxy-constructor
test(Proxy);

let expectedNumTests = 34;
if (numTests != expectedNumTests) {
    errors += "Not all tests were run: ran " + numTests + " out of " + expectedNumTests + " \n";
}
if (errors.length)
    throw new Error(errors);
