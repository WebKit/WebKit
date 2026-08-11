// Comprehensive LEB128 decode verification for IPInt
// Tests i32.const (signed LEB128 i32) and i64.const (signed LEB128 i64)

// Helper: build a wasm module that returns a constant
function makeI32ConstModule(bytes) {
    // (func (result i32) (i32.const <bytes>) )
    let code = [0x00, 0x41, ...bytes, 0x0b]; // no locals, i32.const, end
    let funcBody = [code.length, ...code];
    let wasmBytes = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
        0x0a, funcBody.length + 1, 0x01, ...funcBody
    ]);
    return new WebAssembly.Instance(new WebAssembly.Module(wasmBytes.buffer));
}

function makeI64ConstModule(bytes) {
    // (func (result i64) (i64.const <bytes>) )
    let code = [0x00, 0x42, ...bytes, 0x0b]; // no locals, i64.const, end
    let funcBody = [code.length, ...code];
    let wasmBytes = new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7e,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
        0x0a, funcBody.length + 1, 0x01, ...funcBody
    ]);
    return new WebAssembly.Instance(new WebAssembly.Module(wasmBytes.buffer));
}

// Encode signed LEB128 for i32
function encodeSLEB128_i32(value) {
    value = value | 0; // ensure i32
    let bytes = [];
    while (true) {
        let b = value & 0x7f;
        value >>= 7;
        if ((value === 0 && (b & 0x40) === 0) || (value === -1 && (b & 0x40) !== 0)) {
            bytes.push(b);
            break;
        }
        bytes.push(b | 0x80);
    }
    return bytes;
}

// Encode signed LEB128 for i64 (using BigInt)
function encodeSLEB128_i64(value) {
    value = BigInt(value);
    let bytes = [];
    while (true) {
        let b = Number(value & 0x7fn);
        value >>= 7n;
        if ((value === 0n && (b & 0x40) === 0) || (value === -1n && (b & 0x40) !== 0)) {
            bytes.push(b);
            break;
        }
        bytes.push(b | 0x80);
    }
    return bytes;
}

let failures = 0;

function testI32(value, label) {
    let bytes = encodeSLEB128_i32(value);
    let m = makeI32ConstModule(bytes);
    let result = m.exports.f();
    let expected = value | 0;
    if (result !== expected) {
        print("FAIL i32 " + label + ": value=" + value + " bytes=[" + bytes.map(b=>"0x"+b.toString(16)).join(",") + "] expected=" + expected + " got=" + result);
        failures++;
    }
}

function testI64(value, label) {
    let bytes = encodeSLEB128_i64(value);
    let m = makeI64ConstModule(bytes);
    let result = m.exports.f();
    let expected = BigInt(value);
    if (result !== expected) {
        print("FAIL i64 " + label + ": value=" + value + " bytes=[" + bytes.map(b=>"0x"+b.toString(16)).join(",") + "] expected=" + expected + " got=" + result);
        failures++;
    }
}

// === i32.const tests ===

// Single byte (1 byte LEB128): values -64 to 63
testI32(0, "zero");
testI32(1, "one");
testI32(-1, "neg_one");
testI32(63, "max_single_pos");      // 0x3F — last positive single-byte
testI32(-64, "min_single_neg");     // 0x40 — last negative single-byte
testI32(42, "42");

// Two bytes: values -8192 to 8191
testI32(64, "first_two_byte_pos");  // 0xC0 0x00
testI32(-65, "first_two_byte_neg"); // 0xBF 0x7F
testI32(127, "127");
testI32(-128, "-128");
testI32(128, "128");
testI32(8191, "max_two_byte_pos");
testI32(-8192, "min_two_byte_neg");

// Three bytes
testI32(8192, "first_three_byte");
testI32(-8193, "first_three_byte_neg");
testI32(65535, "65535");
testI32(-65535, "-65535");

// Four bytes
testI32(1048576, "1M");
testI32(-1048576, "-1M");
testI32(16777215, "16M-1");

// Five bytes (max for i32)
testI32(2147483647, "INT32_MAX");
testI32(-2147483648, "INT32_MIN");
testI32(1717661556, "1717661556");   // The value that was failing before
testI32(-1923807898, "-1923807898");
testI32(219737259, "219737259");
testI32(2371159398 | 0, "2371159398_as_i32"); // wraps to negative

// === i64.const tests ===

// Single byte
testI64(0n, "zero");
testI64(1n, "one");
testI64(-1n, "neg_one");
testI64(63n, "max_single_pos");
testI64(-64n, "min_single_neg");

// Multi-byte
testI64(64n, "first_two_byte");
testI64(-65n, "first_two_byte_neg");
testI64(128n, "128");
testI64(-128n, "-128");

// Large positive
testI64(2147483647n, "INT32_MAX");
testI64(2147483648n, "INT32_MAX+1");
testI64(4294967295n, "UINT32_MAX");
testI64(4294967296n, "UINT32_MAX+1");

// Large negative
testI64(-2147483648n, "INT32_MIN");
testI64(-2147483649n, "INT32_MIN-1");

// 64-bit range
testI64(9223372036854775807n, "INT64_MAX");
testI64(-9223372036854775808n, "INT64_MIN");
testI64(18231657398634828518n - (1n << 64n), "large_neg_from_test"); // The value from the failing test
testI64(5825195283807165538n, "large_pos_from_test");

// Values near sign extension boundaries (shift = 7, 14, 21, 28, 35, 42, 49, 56, 63)
testI64(0x40n, "sign_bit_shift7");         // bit 6 set at shift 0
testI64(0x2000n, "sign_bit_shift14");      // bit 13 set
testI64(0x100000n, "sign_bit_shift21");
testI64(0x8000000n, "sign_bit_shift28");
testI64(0x400000000n, "sign_bit_shift35");
testI64(-0x40n, "neg_sign_bit_shift7");
testI64(-0x2000n, "neg_sign_bit_shift14");
testI64(-0x100000n, "neg_sign_bit_shift21");
testI64(-0x8000000n, "neg_sign_bit_shift28");
testI64(-0x400000000n, "neg_sign_bit_shift35");

// === Non-canonical (padded) LEB128 tests ===
// Non-canonical LEBs use more bytes than necessary by adding redundant
// continuation bytes with sign-extension padding. These are valid per the
// WebAssembly spec and must decode to the same value.

// Pad a canonical signed LEB128 encoding to targetLength bytes.
// For non-negative values (bit 6 of last byte clear), pad with 0x00.
// For negative values (bit 6 of last byte set), pad with 0x7f.
function padSLEB128(canonicalBytes, targetLength) {
    if (canonicalBytes.length >= targetLength)
        return canonicalBytes;
    let padded = [...canonicalBytes];
    while (padded.length < targetLength) {
        let lastByte = padded[padded.length - 1];
        let signExtByte = (lastByte & 0x40) ? 0x7f : 0x00;
        padded[padded.length - 1] = lastByte | 0x80; // set continuation bit
        padded.push(signExtByte); // sign-extension terminator
    }
    return padded;
}

function testI32Padded(value, padLen, label) {
    let canonical = encodeSLEB128_i32(value);
    let padded = padSLEB128(canonical, padLen);
    let m = makeI32ConstModule(padded);
    let result = m.exports.f();
    let expected = value | 0;
    if (result !== expected) {
        print("FAIL i32 padded " + label + ": value=" + value + " canonical=[" + canonical.map(b=>"0x"+b.toString(16)).join(",") + "] padded=[" + padded.map(b=>"0x"+b.toString(16)).join(",") + "] expected=" + expected + " got=" + result);
        failures++;
    }
}

function testI64Padded(value, padLen, label) {
    let canonical = encodeSLEB128_i64(value);
    let padded = padSLEB128(canonical, padLen);
    let m = makeI64ConstModule(padded);
    let result = m.exports.f();
    let expected = BigInt(value);
    if (result !== expected) {
        print("FAIL i64 padded " + label + ": value=" + value + " canonical=[" + canonical.map(b=>"0x"+b.toString(16)).join(",") + "] padded=[" + padded.map(b=>"0x"+b.toString(16)).join(",") + "] expected=" + expected + " got=" + result);
        failures++;
    }
}

// i32 non-canonical: pad up to 5 bytes (max for i32 signed LEB128)

// Zero with all possible padding lengths
testI32Padded(0, 2, "zero_2byte");    // [0x80, 0x00] instead of [0x00]
testI32Padded(0, 3, "zero_3byte");    // [0x80, 0x80, 0x00]
testI32Padded(0, 4, "zero_4byte");    // [0x80, 0x80, 0x80, 0x00]
testI32Padded(0, 5, "zero_5byte");    // [0x80, 0x80, 0x80, 0x80, 0x00]

// -1 with all possible padding lengths
testI32Padded(-1, 2, "neg1_2byte");   // [0xff, 0x7f] instead of [0x7f]
testI32Padded(-1, 3, "neg1_3byte");   // [0xff, 0xff, 0x7f]
testI32Padded(-1, 4, "neg1_4byte");   // [0xff, 0xff, 0xff, 0x7f]
testI32Padded(-1, 5, "neg1_5byte");   // [0xff, 0xff, 0xff, 0xff, 0x7f]

// Small positive values padded to max
testI32Padded(1, 5, "one_5byte");
testI32Padded(42, 5, "42_5byte");
testI32Padded(63, 5, "63_5byte");      // max single-byte positive, padded

// Small negative values padded to max
testI32Padded(-2, 5, "neg2_5byte");
testI32Padded(-64, 5, "neg64_5byte");  // max single-byte negative, padded

// Two-byte canonical values padded to various lengths
testI32Padded(64, 3, "64_3byte");
testI32Padded(64, 5, "64_5byte");
testI32Padded(-65, 3, "neg65_3byte");
testI32Padded(-65, 5, "neg65_5byte");
testI32Padded(127, 5, "127_5byte");
testI32Padded(-128, 5, "neg128_5byte");
testI32Padded(8191, 5, "8191_5byte");
testI32Padded(-8192, 5, "neg8192_5byte");

// Three-byte canonical values padded
testI32Padded(8192, 4, "8192_4byte");
testI32Padded(8192, 5, "8192_5byte");
testI32Padded(-8193, 5, "neg8193_5byte");
testI32Padded(65535, 5, "65535_5byte");

// Four-byte canonical values padded to 5
testI32Padded(1048576, 5, "1M_5byte");
testI32Padded(-1048576, 5, "neg1M_5byte");

// i64 non-canonical: pad up to 10 bytes (max for i64 signed LEB128)

// Zero with various padding lengths
testI64Padded(0n, 2, "zero_2byte");
testI64Padded(0n, 5, "zero_5byte");
testI64Padded(0n, 10, "zero_10byte");

// -1 with various padding lengths
testI64Padded(-1n, 2, "neg1_2byte");
testI64Padded(-1n, 5, "neg1_5byte");
testI64Padded(-1n, 10, "neg1_10byte");

// Small values padded to max
testI64Padded(1n, 10, "one_10byte");
testI64Padded(-2n, 10, "neg2_10byte");
testI64Padded(63n, 10, "63_10byte");
testI64Padded(-64n, 10, "neg64_10byte");

// Values that cross byte boundaries, padded
testI64Padded(64n, 10, "64_10byte");
testI64Padded(-65n, 10, "neg65_10byte");
testI64Padded(8192n, 10, "8192_10byte");
testI64Padded(-8193n, 10, "neg8193_10byte");

// 32-bit range values padded in 64-bit encoding
testI64Padded(2147483647n, 10, "INT32_MAX_10byte");
testI64Padded(-2147483648n, 10, "INT32_MIN_10byte");
testI64Padded(4294967295n, 10, "UINT32_MAX_10byte");

// Large 64-bit values padded (these are already 9-10 byte canonical, but test padding where possible)
testI64Padded(0x400000000n, 10, "shift35_10byte");
testI64Padded(-0x400000000n, 10, "neg_shift35_10byte");
testI64Padded(0x20000000000n, 10, "shift42_10byte");
testI64Padded(-0x20000000000n, 10, "neg_shift42_10byte");

if (failures !== 0) {
    print(failures + " tests FAILED");
    throw new Error(failures + " LEB128 decode failures");
}
