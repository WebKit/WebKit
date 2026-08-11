//@ requireOptions("--useWasmSIMD=1", "--useWasmRelaxedSIMD=1")
//@ skip if !$isSIMDPlatform
import * as assert from "../assert.js"

// Test i16x8.relaxed_dot_i8x16_i7x16_s and i32x4.relaxed_dot_i8x16_i7x16_add_s
// using hand-crafted wasm binary (wabt does not support these opcodes)

// Helper to encode unsigned LEB128
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

// Wasm SIMD opcodes (all prefixed with 0xfd)
const V128_CONST = [0xfd, ...leb128(0x0c)];
const I16X8_EXTRACT_LANE_S = (lane) => [0xfd, ...leb128(0x18), lane];
const I32X4_EXTRACT_LANE = (lane) => [0xfd, ...leb128(0x1b), lane];
const I16X8_RELAXED_DOT = [0xfd, ...leb128(0x112)];
const I32X4_RELAXED_DOT_ADD = [0xfd, ...leb128(0x113)];

// Test vectors (i8x16 as byte arrays, little-endian)
// a = [1, 2, 3, 4, 5, 6, 7, 8, -1, -2, 127, -128, 0, 0, 10, 20]
const vecA = [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
              0xFF, 0xFE, 0x7F, 0x80, 0x00, 0x00, 0x0A, 0x14];
// b = [1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 1, 1, 0, 0, 10, 20] (all in i7 range: 0-127)
const vecB = [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
              0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x0A, 0x14];
// accumulator c = [100, 200, 300, 400] as i32x4
const vecC = [0x64, 0x00, 0x00, 0x00, 0xC8, 0x00, 0x00, 0x00,
              0x2C, 0x01, 0x00, 0x00, 0x90, 0x01, 0x00, 0x00];

function buildSection(id, content) {
    const flat = content.flat();
    return [id, ...leb128(flat.length), ...flat];
}

function buildFuncBody(code) {
    const body = [0x00, ...code.flat(), 0x0b]; // 0 locals + code + end
    return [...leb128(body.length), ...body];
}

function buildExport(name, kind, index) {
    const nameBytes = [...name].map(c => c.charCodeAt(0));
    return [...leb128(nameBytes.length), ...nameBytes, kind, ...leb128(index)];
}

// Build function bodies
const funcDotLane0 = buildFuncBody([
    V128_CONST, vecA, V128_CONST, vecB,
    I16X8_RELAXED_DOT, I16X8_EXTRACT_LANE_S(0),
]);
const funcDotLane4 = buildFuncBody([
    V128_CONST, vecA, V128_CONST, vecB,
    I16X8_RELAXED_DOT, I16X8_EXTRACT_LANE_S(4),
]);
const funcDotLane5 = buildFuncBody([
    V128_CONST, vecA, V128_CONST, vecB,
    I16X8_RELAXED_DOT, I16X8_EXTRACT_LANE_S(5),
]);
const funcDotLane7 = buildFuncBody([
    V128_CONST, vecA, V128_CONST, vecB,
    I16X8_RELAXED_DOT, I16X8_EXTRACT_LANE_S(7),
]);
const funcDotAddLane0 = buildFuncBody([
    V128_CONST, vecA, V128_CONST, vecB, V128_CONST, vecC,
    I32X4_RELAXED_DOT_ADD, I32X4_EXTRACT_LANE(0),
]);
const funcDotAddLane2 = buildFuncBody([
    V128_CONST, vecA, V128_CONST, vecB, V128_CONST, vecC,
    I32X4_RELAXED_DOT_ADD, I32X4_EXTRACT_LANE(2),
]);
const funcDotAddLane3 = buildFuncBody([
    V128_CONST, vecA, V128_CONST, vecB, V128_CONST, vecC,
    I32X4_RELAXED_DOT_ADD, I32X4_EXTRACT_LANE(3),
]);

// Assemble the module
const wasmBytes = new Uint8Array([
    // Header
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    // Type section: 1 type - func () -> i32
    ...buildSection(0x01, [0x01, 0x60, 0x00, 0x01, 0x7f]),
    // Function section: 7 functions, all type 0
    ...buildSection(0x03, [0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]),
    // Export section
    ...buildSection(0x07, [
        0x07,
        ...buildExport("test_dot_lane0", 0x00, 0),
        ...buildExport("test_dot_lane4", 0x00, 1),
        ...buildExport("test_dot_lane5", 0x00, 2),
        ...buildExport("test_dot_lane7", 0x00, 3),
        ...buildExport("test_dot_add_lane0", 0x00, 4),
        ...buildExport("test_dot_add_lane2", 0x00, 5),
        ...buildExport("test_dot_add_lane3", 0x00, 6),
    ]),
    // Code section
    ...buildSection(0x0a, [
        0x07,
        ...funcDotLane0,
        ...funcDotLane4,
        ...funcDotLane5,
        ...funcDotLane7,
        ...funcDotAddLane0,
        ...funcDotAddLane2,
        ...funcDotAddLane3,
    ]),
]);

const result = await WebAssembly.instantiate(wasmBytes);
const exports = result.instance.exports;

// i16x8.relaxed_dot_i8x16_i7x16_s: output[i] = a[2i]*b[2i] + a[2i+1]*b[2i+1]
// lane 0: 1*1 + 2*2 = 5
// lane 4: (-1)*1 + (-2)*2 = -5
// lane 5: 127*1 + (-128)*1 = -1
// lane 7: 10*10 + 20*20 = 500
//
// i32x4.relaxed_dot_i8x16_i7x16_add_s: sums adjacent i16 dot products + accumulator
// lane 0: (1*1+2*2) + (3*3+4*4) + 100 = 5 + 25 + 100 = 130
// lane 2: ((-1)*1+(-2)*2) + (127*1+(-128)*1) + 300 = -5 + (-1) + 300 = 294
// lane 3: (0*0+0*0) + (10*10+20*20) + 400 = 0 + 500 + 400 = 900

for (let i = 0; i < wasmTestLoopCount; ++i) {
    assert.eq(exports.test_dot_lane0(), 5);
    assert.eq(exports.test_dot_lane4(), -5);
    assert.eq(exports.test_dot_lane5(), -1);
    assert.eq(exports.test_dot_lane7(), 500);
    assert.eq(exports.test_dot_add_lane0(), 130);
    assert.eq(exports.test_dot_add_lane2(), 294);
    assert.eq(exports.test_dot_add_lane3(), 900);
}
