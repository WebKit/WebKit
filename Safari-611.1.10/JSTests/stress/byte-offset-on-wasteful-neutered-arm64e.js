function foo(o) {
    return o.byteOffset;
}
noInline(foo);

function assert(b) {
    if (!b)
        throw new Error;
}

var array = new Int8Array(new ArrayBuffer(100));

for (let i = 0; i < 1000; ++i) {
    assert(foo(array) === 0);
}

transferArrayBuffer(array.buffer)

for (let i = 0; i < 10000; ++i) {
    assert(foo(array) === 0);
}
