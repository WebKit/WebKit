function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function isView(value)
{
    return ArrayBuffer.isView(value);
}
noInline(isView);

let buffer = new ArrayBuffer(16);
let otherGlobalObject = createGlobalObject();
class DerivedUint8Array extends Uint8Array { }
class DerivedDataView extends DataView { }

let cases = [
    [ new Int8Array(1), true ],
    [ new Uint8Array(1), true ],
    [ new Uint8ClampedArray(1), true ],
    [ new Int16Array(1), true ],
    [ new Uint16Array(1), true ],
    [ new Int32Array(1), true ],
    [ new Uint32Array(1), true ],
    [ new Float16Array(1), true ],
    [ new Float32Array(1), true ],
    [ new Float64Array(1), true ],
    [ new BigInt64Array(1), true ],
    [ new BigUint64Array(1), true ],
    [ new DataView(buffer), true ],
    [ new DerivedUint8Array(1), true ],
    [ new DerivedDataView(buffer), true ],
    [ new otherGlobalObject.Uint8Array(1), true ],
    [ new otherGlobalObject.DataView(new otherGlobalObject.ArrayBuffer(8)), true ],
    [ new Proxy(new Uint8Array(1), {}), false ],
    [ buffer, false ],
    [ new SharedArrayBuffer(8), false ],
    [ [], false ],
    [ {}, false ],
    [ { buffer, byteLength: 16, byteOffset: 0 }, false ],
    [ 42, false ],
    [ 4.2, false ],
    [ "text", false ],
    [ Symbol("symbol"), false ],
    [ 42n, false ],
    [ true, false ],
    [ null, false ],
    [ undefined, false ],
];

let detached = new Uint8Array(4);
transferArrayBuffer(detached.buffer);
cases.push([ detached, true ]);

for (let i = 0; i < 1e4; ++i) {
    for (let pair of cases)
        shouldBe(isView(pair[0]), pair[1]);
    shouldBe(isView(), false);
}

function isViewOfTypedArray()
{
    return ArrayBuffer.isView(new Uint8Array(1));
}
noInline(isViewOfTypedArray);

function isViewOfDataView()
{
    return ArrayBuffer.isView(new DataView(new ArrayBuffer(4)));
}
noInline(isViewOfDataView);

function isViewOfArrayBuffer()
{
    return ArrayBuffer.isView(new ArrayBuffer(4));
}
noInline(isViewOfArrayBuffer);

for (let i = 0; i < 1e4; ++i) {
    shouldBe(isViewOfTypedArray(), true);
    shouldBe(isViewOfDataView(), true);
    shouldBe(isViewOfArrayBuffer(), false);
}
