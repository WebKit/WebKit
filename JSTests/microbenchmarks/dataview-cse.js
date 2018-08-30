"use strict";

function assert(b) {
    if (!b)
        throw new Error;
}

function test(dv, littleEndian) {
    return dv.getInt32(0, littleEndian)
    + dv.getInt32(0, littleEndian)
    + dv.getInt32(0, littleEndian)
    + dv.getInt32(0, littleEndian)
    + dv.getInt32(0, littleEndian)
    + dv.getInt32(0, littleEndian)
    + dv.getInt32(0, littleEndian)
    + dv.getInt32(0, littleEndian)
    + dv.getInt32(0, littleEndian)
    + dv.getInt32(0, littleEndian)
}
noInline(test);

let ab = new ArrayBuffer(4);
let dv = new DataView(ab);
dv.setInt32(0, 10, true);
for (let i = 0; i < 1000000; ++i) {
    let result = test(dv, true);
    assert(result === 10*10);
}
