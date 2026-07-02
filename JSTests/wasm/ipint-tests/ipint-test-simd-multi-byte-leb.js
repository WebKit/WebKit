import * as assert from "../assert.js"

// Test SIMD instructions with padded (non-minimal) LEB128 opcode encoding.
// The simd_prefix handler decodes the opcode LEB128 into t4, so t4 correctly
// points past however many bytes the opcode takes. Before this fix, the code
// used a hardcoded offset from PC (ImmLaneIdxOffset = 2), which assumed
// a 1-byte opcode. With padded LEB128, the opcode is 2+ bytes, making the
// hardcoded offset wrong. This test verifies the t4-based approach works.

function buildModule(codeBody) {
    // Minimal wasm module: 1 type (() -> i32), 1 func, 1 export "f"
    const header = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    const typeSection = [0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f]; // () -> i32
    const funcSection = [0x03, 0x02, 0x01, 0x00];
    const exportSection = [0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00]; // "f"

    const bodyWithLocals = [0x00, ...codeBody, 0x0b]; // 0 locals, body, end
    const bodyLen = bodyWithLocals.length;
    const codeSection = [0x0a, ...uleb128(bodyLen + 1 + uleb128(bodyLen).length), 0x01, ...uleb128(bodyLen), ...bodyWithLocals];

    return new Uint8Array([...header, ...typeSection, ...funcSection, ...exportSection, ...codeSection]);
}

function uleb128(value) {
    const bytes = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value !== 0) byte |= 0x80;
        bytes.push(byte);
    } while (value !== 0);
    return bytes;
}

// Encode a SIMD opcode with padded LEB128 (add continuation byte).
// Opcode 0x16 -> [0xfd, 0x96, 0x00] instead of [0xfd, 0x16]
// Opcode 0xAE (already 2-byte: 0xAE,0x01) -> [0xfd, 0xAE, 0x81, 0x00] (3-byte padded)
function paddedSIMDOp(opcode) {
    // Normal LEB128 encoding, then pad with one extra continuation byte
    const lebBytes = uleb128(opcode);
    // Set continuation bit on the last byte, then add 0x00
    lebBytes[lebBytes.length - 1] |= 0x80;
    lebBytes.push(0x00);
    return [0xfd, ...lebBytes];
}

// v128.const i32x4 with specified values
function v128ConstI32x4(a, b, c, d) {
    const buf = new ArrayBuffer(16);
    const view = new DataView(buf);
    view.setInt32(0, a, true);
    view.setInt32(4, b, true);
    view.setInt32(8, c, true);
    view.setInt32(12, d, true);
    return [0xfd, 0x0c, ...new Uint8Array(buf)];
}

// v128.const with padded opcode
function v128ConstI32x4Padded(a, b, c, d) {
    const buf = new ArrayBuffer(16);
    const view = new DataView(buf);
    view.setInt32(0, a, true);
    view.setInt32(4, b, true);
    view.setInt32(8, c, true);
    view.setInt32(12, d, true);
    return [...paddedSIMDOp(0x0c), ...new Uint8Array(buf)];
}

// i32.const with correct signed LEB128 encoding
function i32Const(value) {
    const bytes = [];
    let more = true;
    while (more) {
        let byte = value & 0x7f;
        value >>= 7;
        if ((value === 0 && (byte & 0x40) === 0) || (value === -1 && (byte & 0x40) !== 0))
            more = false;
        else
            byte |= 0x80;
        bytes.push(byte);
    }
    return [0x41, ...bytes];
}

function instantiateAndRun(bytes) {
    const mod = new WebAssembly.Module(bytes);
    const inst = new WebAssembly.Instance(mod);
    return inst.exports.f();
}

// Test 1: i8x16.extract_lane_u (opcode 0x16) with padded LEB128
// Extracts byte at lane 4 from i32x4 [0x04030201, ...]
// Lane 4 = first byte of second i32 = 0x05
{
    const bytes = buildModule([
        ...v128ConstI32x4(0x04030201, 0x08070605, 0x0c0b0a09, 0x100f0e0d),
        ...paddedSIMDOp(0x16), 0x04, // i8x16.extract_lane_u 4 (padded opcode)
    ]);
    assert.eq(instantiateAndRun(bytes), 5);
}

// Test 2: i32x4.extract_lane (opcode 0x1b) with padded LEB128
{
    const bytes = buildModule([
        ...v128ConstI32x4(10, 20, 30, 40),
        ...paddedSIMDOp(0x1b), 0x02, // i32x4.extract_lane 2 (padded opcode)
    ]);
    assert.eq(instantiateAndRun(bytes), 30);
}

// Test 3: v128.const (opcode 0x0c) with padded LEB128
{
    const bytes = buildModule([
        ...v128ConstI32x4Padded(42, 0, 0, 0), // v128.const with padded opcode
        0xfd, 0x1b, 0x00, // i32x4.extract_lane 0 (normal encoding)
    ]);
    assert.eq(instantiateAndRun(bytes), 42);
}

// Test 4: i8x16.replace_lane (opcode 0x17) with padded LEB128
{
    const bytes = buildModule([
        ...v128ConstI32x4(0, 0, 0, 0),
        ...i32Const(99),             // i32.const 99 (proper signed LEB128)
        ...paddedSIMDOp(0x17), 0x02, // i8x16.replace_lane 2 (padded opcode)
        ...paddedSIMDOp(0x16), 0x02, // i8x16.extract_lane_u 2 (padded opcode)
    ]);
    assert.eq(instantiateAndRun(bytes), 99);
}

// Test 5: Chain of padded ops — v128.const (padded) + i32x4.add (padded, opcode 0xAE)
// + i32x4.extract_lane (padded)
{
    const bytes = buildModule([
        ...v128ConstI32x4Padded(10, 20, 30, 40),
        ...v128ConstI32x4Padded(1, 2, 3, 4),
        ...paddedSIMDOp(0xAE),       // i32x4.add (padded)
        ...paddedSIMDOp(0x1b), 0x01, // i32x4.extract_lane 1 (padded opcode)
    ]);
    assert.eq(instantiateAndRun(bytes), 22);
}

// Test 6: i8x16.shuffle (opcode 0x0d) with padded LEB128
// Shuffle: take first 4 bytes from second vector, then first 12 from first.
{
    const shuffleImm = [
        0x10, 0x11, 0x12, 0x13,  // lanes 16-19 (from second vector)
        0x00, 0x01, 0x02, 0x03,  // lanes 0-3 (from first vector)
        0x04, 0x05, 0x06, 0x07,  // lanes 4-7 (from first vector)
        0x08, 0x09, 0x0a, 0x0b,  // lanes 8-11 (from first vector)
    ];
    const bytes = buildModule([
        ...v128ConstI32x4(0x04030201, 0x08070605, 0x0c0b0a09, 0x100f0e0d),
        ...v128ConstI32x4(0x44434241, 0x48474645, 0x4c4b4a49, 0x504f4e4d),
        ...paddedSIMDOp(0x0d), ...shuffleImm,  // i8x16.shuffle (padded opcode)
        0xfd, 0x1b, 0x00, // i32x4.extract_lane 0 (normal encoding)
    ]);
    // First 4 bytes of result = bytes 16-19 = first 4 bytes of second vector = 0x44434241
    assert.eq(instantiateAndRun(bytes), 0x44434241);
}
