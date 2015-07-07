var a = new Int32Array(new ArrayBuffer(100), 4, 1);

if (a.length != 1)
    throw "Error: bad length: " + a.length;
if (a.byteOffset != 4)
    throw "Error: bad offset: " + a.byteOffset;
if (a.byteLength != 4)
    throw "Error: bad byte length: " + a.byteLength;

function foo() {
    if (a.length != 1)
        throw "Error: bad length: " + a.length;
    if (a.byteOffset != 4)
        throw "Error: bad offset: " + a.byteOffset;
    if (a.byteLength != 4)
        throw "Error: bad byte length: " + a.byteLength;
}

for (var i = 0; i < 1000000; ++i)
    foo();

transferArrayBuffer(a.buffer);

var didThrow = false;
try {
    foo();
} catch (e) {
    didThrow = true;
}

if (!didThrow)
    throw "Should have thrown.";

if (a.length != 0)
    throw "Error: bad length: " + a.length;
if (a.byteOffset != 0)
    throw "Error: bad offset: " + a.byteOffset;
if (a.byteLength != 0)
    throw "Error: bad byte length: " + a.byteLength;
