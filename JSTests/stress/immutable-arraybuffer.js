//@ runDefault("--useImmutableArrayBuffer=1")

// Basic coverage for the immutable ArrayBuffer proposal:
// https://tc39.es/proposal-immutable-arraybuffer/
// Conformance details are covered by test262; this file focuses on JSC internals:
// view modes, JIT tier transitions for stores, and shell detach interaction.

function assert(cond, message) {
    if (!cond)
        throw new Error("assertion failed: " + message);
}

function assertEq(actual, expected, message) {
    if (actual !== expected)
        throw new Error("assertion failed: " + message + " (expected " + expected + ", got " + actual + ")");
}

function assertThrows(fn, errorType, message) {
    let threw = false;
    try {
        fn();
    } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error("wrong exception type for " + message + ": " + e);
    }
    if (!threw)
        throw new Error("did not throw: " + message);
}

{
    // transferToImmutable basics.
    let ab = new ArrayBuffer(8);
    new Uint8Array(ab).set([1, 2, 3, 4, 5, 6, 7, 8]);
    assertEq(ab.immutable, false, "mutable by default");
    let iab = ab.transferToImmutable();
    assertEq(ab.detached, true, "source is detached");
    assertEq(iab.immutable, true, "result is immutable");
    assertEq(iab.resizable, false, "immutable is not resizable");
    assertEq(iab.maxByteLength, iab.byteLength, "maxByteLength == byteLength");
    assertEq(iab.byteLength, 8, "length preserved");
    assertEq(new Uint8Array(iab)[3], 4, "contents preserved");

    // Growing transfer.
    let grown = new ArrayBuffer(4).transferToImmutable(16);
    assertEq(grown.byteLength, 16, "grown length");
    assertEq(grown.immutable, true, "grown immutable");

    // Shrinking transfer.
    let shrunk = new ArrayBuffer(16).transferToImmutable(4);
    assertEq(shrunk.byteLength, 4, "shrunk length");

    // Resizable source.
    let rab = new ArrayBuffer(8, { maxByteLength: 32 });
    let irab = rab.transferToImmutable();
    assertEq(irab.immutable, true, "from resizable");
    assertEq(irab.resizable, false, "result not resizable");
    assertEq(irab.maxByteLength, 8, "result fixed");

    // Operations that must reject immutable buffers.
    assertThrows(() => iab.transfer(), TypeError, "transfer on immutable");
    assertThrows(() => iab.transferToFixedLength(), TypeError, "transferToFixedLength on immutable");
    assertThrows(() => iab.transferToImmutable(), TypeError, "transferToImmutable on immutable");
    assertThrows(() => iab.resize(16), TypeError, "resize on immutable");
    assertThrows(() => transferArrayBuffer(iab), TypeError, "shell detach on immutable");
}

{
    // sliceToImmutable basics.
    let ab = new ArrayBuffer(8);
    new Uint8Array(ab).set([10, 11, 12, 13, 14, 15, 16, 17]);
    let slice = ab.sliceToImmutable(2, 6);
    assertEq(slice.immutable, true, "slice is immutable");
    assertEq(slice.byteLength, 4, "slice length");
    assertEq(new Uint8Array(slice)[0], 12, "slice contents");
    assertEq(ab.detached, false, "source not detached");

    // Snapshot semantics: later mutation does not affect the slice.
    new Uint8Array(ab)[2] = 99;
    assertEq(new Uint8Array(slice)[0], 12, "slice is a snapshot");

    // sliceToImmutable on an immutable buffer works.
    let slice2 = slice.sliceToImmutable(1, 3);
    assertEq(slice2.byteLength, 2, "slice of immutable");
    assertEq(new Uint8Array(slice2)[0], 13, "slice of immutable contents");
}

{
    // Views over immutable buffers: reads work, writes fail.
    let iab = new ArrayBuffer(16).transferToImmutable();
    let ta = new Int32Array(iab);
    ta; // view creation is fine
    assertEq(ta[0], 0, "read works");

    // Sloppy-mode store silently fails.
    (function() { ta[0] = 42; })();
    assertEq(ta[0], 0, "sloppy store is a no-op");

    // Strict-mode store throws.
    assertThrows(() => { "use strict"; ta[0] = 42; }, TypeError, "strict store throws");
    assertThrows(() => { "use strict"; ta[100] = 42; }, TypeError, "strict out-of-bounds store throws too");

    // Descriptors report non-writable, non-configurable.
    let desc = Object.getOwnPropertyDescriptor(ta, "0");
    assertEq(desc.writable, false, "descriptor writable");
    assertEq(desc.enumerable, true, "descriptor enumerable");
    assertEq(desc.configurable, false, "descriptor configurable");

    // Object.freeze works on immutable-backed typed arrays.
    Object.freeze(ta);

    // defineOwnProperty: no-op redefinition succeeds, changes fail.
    Object.defineProperty(ta, "0", { writable: false });
    assertThrows(() => Object.defineProperty(ta, "0", { value: 42 }), TypeError, "defineProperty value");

    // Mutating prototype methods throw.
    assertThrows(() => ta.fill(1), TypeError, "fill");
    assertThrows(() => ta.copyWithin(0, 1), TypeError, "copyWithin");
    assertThrows(() => ta.reverse(), TypeError, "reverse");
    assertThrows(() => ta.sort(), TypeError, "sort");
    assertThrows(() => ta.set([1, 2]), TypeError, "set");
    assertThrows(() => new Uint8Array(iab).set([1]), TypeError, "set Uint8");

    // DataView setters throw, getters work.
    let dv = new DataView(iab);
    assertEq(dv.getUint8(0), 0, "DataView get works");
    assertThrows(() => dv.setUint8(0, 1), TypeError, "DataView setUint8");
    assertThrows(() => dv.setFloat64(0, 1.5), TypeError, "DataView setFloat64");

    // Atomics: mutators throw, readers work.
    let i32 = new Int32Array(iab);
    assertThrows(() => Atomics.store(i32, 0, 1), TypeError, "Atomics.store");
    assertThrows(() => Atomics.add(i32, 0, 1), TypeError, "Atomics.add");
    assertThrows(() => Atomics.compareExchange(i32, 0, 0, 1), TypeError, "Atomics.compareExchange");
    assertEq(Atomics.load(i32, 0), 0, "Atomics.load works");
    assertEq(Atomics.notify(i32, 0), 0, "Atomics.notify returns 0");

    // setFromHex / setFromBase64 throw.
    let u8 = new Uint8Array(iab);
    assertThrows(() => u8.setFromHex("00"), TypeError, "setFromHex");
    assertThrows(() => u8.setFromBase64("AA=="), TypeError, "setFromBase64");

    // Reads through views and non-mutating methods work.
    assertEq(i32.length, 4, "length");
    assertEq(i32.slice(0, 2).length, 2, "slice produces mutable copy");
    i32.slice(0, 2)[0] = 7; // mutable copy is writable
    assertEq(i32.subarray(0, 2).buffer.immutable, true, "subarray shares the immutable buffer");
}

{
    // Stores through all JIT tiers. The store must never write, across tiers.
    let iab = new ArrayBuffer(8).transferToImmutable();
    let view = new Uint8Array(iab);
    function storeSloppy(v, i, x) { v[i] = x; }
    function storeStrict(v, i, x) { "use strict"; v[i] = x; }
    function storeMut(v, i, x) { "use strict"; let m = new Uint8Array(8); m[i] = x; return m[i]; }

    for (let i = 0; i < testLoopCount; ++i) {
        storeSloppy(view, 0, 1);
        assertEq(view[0], 0, "sloppy store never writes (iteration " + i + ")");
        assertThrows(() => storeStrict(view, 0, 1), TypeError, "strict store throws (iteration " + i + ")");
        // A mutable view at the same call site must still store correctly.
        assertEq(storeMut(view, 1, 5), 5, "mutable store at polymorphic site works");
        storeSloppy(new Uint8Array(8), 0, 3); // keep the site polymorphic
    }
}

{
    // DataView stores through JIT tiers.
    let iab = new ArrayBuffer(8).transferToImmutable();
    let dv = new DataView(iab);
    function dvStore(d, x) { d.setUint32(0, x); }
    for (let i = 0; i < testLoopCount; ++i) {
        assertThrows(() => dvStore(dv, 1), TypeError, "DataView store throws (iteration " + i + ")");
        dvStore(new DataView(new ArrayBuffer(8)), 1); // mutable site stays functional
    }
    assertEq(dv.getUint32(0), 0, "DataView contents unchanged");
}
