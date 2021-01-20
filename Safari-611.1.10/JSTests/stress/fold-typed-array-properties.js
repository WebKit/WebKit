var a = new Int32Array(new ArrayBuffer(100), 4, 1);

if (a.length != 1)
    throw "Error: bad length (start): " + a.length;
if (a.byteOffset != 4)
    throw "Error: bad offset (start): " + a.byteOffset;
if (a.byteLength != 4)
    throw "Error: bad byte length (start): " + a.byteLength;

function foo(when) {
    var tmp = a.length;
    if (tmp != 1)
        throw "Error: bad length (" + when + "): " + tmp;
    tmp = a.byteOffset;
    if (tmp != 4)
        throw "Error: bad offset (" + when + "): " + tmp;
    tmp = a.byteLength;
    if (tmp != 4)
        throw "Error: bad byte length (" + when + "): " + tmp;
}

for (var i = 0; i < 1000000; ++i)
    foo("loop");

transferArrayBuffer(a.buffer);

var didThrow = false;
try {
    foo("after transfer");
} catch (e) {
    didThrow = true;
}

if (!didThrow)
    throw "Should have thrown.";
