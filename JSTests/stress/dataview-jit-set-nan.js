//@ skip if $architecture == "mips"

"use strict";

function assert(b) {
    if (!b)
        throw new Error;
}

function getIsLittleEndian() {
    let ab = new ArrayBuffer(2);
    let ta = new Int16Array(ab);
    ta[0] = 0x0102;
    let dv = new DataView(ab);
    return dv.getInt16(0, true) === 0x0102;
}

let isLittleEndian = getIsLittleEndian();

function adjustForEndianessFloat32(value) {
    if (isLittleEndian)
        return value;

    let ab = new ArrayBuffer(4);
    let ta = new Float32Array(ab);
    ta[0] = value;
    let dv = new DataView(ab);
    return dv.getFloat32(0, true);
}

function test() {
    function storeLittleEndian(dv, index, value) {
        dv.setFloat32(index, value, true);
    }
    noInline(storeLittleEndian);

    function storeBigEndian(dv, index, value) {
        dv.setFloat32(index, value, false);
    }
    noInline(storeBigEndian);

    function store(dv, index, value, littleEndian) {
        dv.setFloat32(index, value, littleEndian);
    }
    noInline(store);

    let buffer = new ArrayBuffer(4);
    let arr = new Float32Array(buffer);
    let bits = new Uint32Array(buffer);
    let dv = new DataView(buffer);

    for (let i = 0; i < 10000; ++i) {
        storeLittleEndian(dv, 0, adjustForEndianessFloat32(1.5));
        assert(arr[0] === 1.5);

        // The right way how to process this test is to uncomment the line below
        // and comment out the line below it. But strangely it doesn't work. I
        // opened https://bugs.webkit.org/show_bug.cgi?id=209289 for it.
        //storeLittleEndian(dv, 0, adjustForEndianessFloat32(12912.124123215122));
        store(dv, 0, 12912.124123215122, isLittleEndian);
        assert(arr[0] === 12912.1240234375);
        assert(bits[0] === 0x4649c07f);

        storeLittleEndian(dv, 0, adjustForEndianessFloat32(NaN));
        assert(isNaN(arr[0]));
        assert(bits[0] === 0x7FC00000);

        storeLittleEndian(dv, 0, adjustForEndianessFloat32(2.3879393e-38));
        assert(arr[0] === 2.387939260590663e-38);
        assert(bits[0] === 0x01020304);

        storeBigEndian(dv, 0, adjustForEndianessFloat32(2.3879393e-38));
        assert(arr[0] === 1.539989614439558e-36);
        assert(bits[0] === 0x04030201);
    }
}
test();
