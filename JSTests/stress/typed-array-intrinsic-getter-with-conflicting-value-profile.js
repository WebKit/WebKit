function foo(x) {
    return x.byteLength
}

var arr = new Uint8Array(42);

var badPrototype = {};
var bad = Object.create(badPrototype);

for (var i = 0; i < 1e6; i++) {
    if (null != foo(bad)) {
        throw new Error();
    }
}

badPrototype.byteLength = 42;

for (var i = 0; i < 1e6; i++) {
    if (42 != foo(arr)) {
        throw new Error();
    }
}
