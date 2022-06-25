description("Tests wrappers for ArrayBuffer objects are not GCed when they shouldn't be");

let types = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array, DataView];

debug("Test subclassing");

types.forEach(function(type) {
    C = class extends ArrayBuffer { }

    let b = new C(8);
    foo = new type(b);

    b = null;
    gc();

    shouldBeTrue("foo.buffer instanceof C");
});

debug("");
debug("Test properties");

types.forEach(function(type) {
    b = new ArrayBuffer(8);
    b.bar = 1;

    foo = new Int32Array(b);
    b = null;
    gc();

    shouldBe("foo.buffer.bar", "1");
});

debug("");
debug("Test WeakMap");

types.forEach(function(type) {
    ws = new WeakSet();
    b = new ArrayBuffer(8);

    ws.add(b);
    foo = new Int32Array(b);
    b = null;

    gc();

    shouldBeTrue("ws.has(foo.buffer)");
});
