//@ requireOptions("--useWasmSIMD=1", "--useWasmRelaxedSIMD=1")
//@ skip if !$isSIMDPlatform
import * as assert from "../assert.js"

function leb128(value) {
    const result = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value !== 0) byte |= 0x80;
        result.push(byte);
    } while (value !== 0);
    return result;
}

function buildSection(id, content) {
    const flat = content.flat();
    return [id, ...leb128(flat.length), ...flat];
}

const LOCAL_GET = (i) => [0x20, i];
const LOCAL_SET = (i) => [0x21, i];
const I32X4_SPLAT = [0xfd, ...leb128(0x11)];
const I32X4_EXTRACT_LANE = (lane) => [0xfd, ...leb128(0x1b), lane];
const I32X4_RELAXED_DOT_ADD = [0xfd, ...leb128(0x113)];

// (func (param i32 i32) (result i32) (local v128)
//   (local.set 2 (i32x4.splat (local.get 0)))
//   (i32x4.extract_lane 0 (i32x4.relaxed_dot_i8x16_i7x16_add_s (local.get 2) (local.get 2) (i32x4.splat (local.get 1)))))
const code = [
    0x01, 0x01, 0x7b,
    LOCAL_GET(0), I32X4_SPLAT, LOCAL_SET(2),
    LOCAL_GET(2), LOCAL_GET(2), LOCAL_GET(1), I32X4_SPLAT,
    I32X4_RELAXED_DOT_ADD, I32X4_EXTRACT_LANE(0),
    0x0b,
].flat();

const wasmBytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    ...buildSection(0x01, [0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f]),
    ...buildSection(0x03, [0x01, 0x00]),
    ...buildSection(0x07, [0x01, 0x04, ..."test".split("").map(c => c.charCodeAt(0)), 0x00, 0x00]),
    ...buildSection(0x0a, [0x01, ...leb128(code.length), ...code]),
]);

const { instance } = await WebAssembly.instantiate(wasmBytes);
const test = instance.exports.test;

for (let i = 0; i < wasmTestLoopCount; ++i) {
    // The bytes of 0x01020304 are 4, 3, 2, 1, so each lane gets 4*4 + 3*3 + 2*2 + 1*1 = 30.
    assert.eq(test(0x01020304, 100), 130);
    assert.eq(test(0x7f7f7f7f, -5), 4 * 127 * 127 - 5);
    assert.eq(test(0, i), i);
}
