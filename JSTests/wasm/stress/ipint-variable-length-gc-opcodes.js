import * as assert from "../assert.js";

/*
 * Test for IPInt variable-length LEB128 sub-opcode handling.
 *
 * This test validates the fix for a bug where certain WebAssembly GC opcodes
 * incorrectly assumed fixed-length instruction encoding. The bug occurred because:
 * - GC opcodes use a two-level encoding: base opcode (0xFB) + sub-opcode (LEB128)
 * - LEB128 allows the same number to be represented multiple ways (e.g., 15 can be 0x0F or 0x8F 0x00)
 * - Some opcodes used advancePC(2) (constant) instead of dynamic length tracking
 * - This caused IPInt to advance by the wrong number of bytes with redundant encodings
 *
 * The test manually constructs WASM binaries with redundantly-encoded sub-opcodes
 * to verify IPInt handles variable-length encodings correctly.
 *
 * Fixed opcodes tested:
 * - ref.i31 (0x1C), i31.get_s (0x1D), i31.get_u (0x1E)
 * - array.len (0x0F), array.fill (0x10), array.copy (0x11)
 * - any.convert_extern (0x1A), extern.convert_any (0x1B)
 */

// Helper function to create redundant LEB128 encoding of a value
// For values < 128, we can add redundant continuation bytes
function createRedundantLEB128(value, totalBytes) {
    if (totalBytes === 1) {
        return [value];
    }

    // Create redundant encoding by adding continuation bytes
    let result = [];
    for (let i = 0; i < totalBytes - 1; i++) {
        if (i === 0) {
            result.push(value | 0x80);  // First byte with continuation bit
        } else {
            result.push(0x80);  // Middle bytes: just continuation bit
        }
    }
    result.push(0x00);  // Final byte: no continuation bit, value 0

    return result;
}

// Helper to calculate LEB128 size for section sizes
function encodeVarUInt32(value) {
    let result = [];
    do {
        let byte = value & 0x7F;
        value >>>= 7;
        if (value !== 0) {
            byte |= 0x80;
        }
        result.push(byte);
    } while (value !== 0);
    return result;
}

// ========================================
// ref.i31 Tests
// ========================================

function testRefI31RedundantEncoding() {
    // Test ref.i31 with normal encoding (1 byte sub-opcode)
    {
        let extendedOp = [0x1C];  // ref.i31 normal
        let codeBody = [
            0x00,  // 0 locals
            0x41, 0x2A,  // i32.const 42
            0xFB, ...extendedOp,  // ref.i31
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            // WASM header
            0x00, 0x61, 0x73, 0x6D,  // Magic
            0x01, 0x00, 0x00, 0x00,  // Version

            // Type section [type 0: () -> i31ref]
            0x01,  // Section ID
            0x05,  // Section size
            0x01,  // 1 type
            0x60,  // Function type
            0x00,  // 0 params
            0x01,  // 1 return
            0x6C,  // i31ref (ref null i31)

            // Function section [function 0 uses type 0]
            0x03,  // Section ID
            0x02,  // Section size
            0x01,  // 1 function
            0x00,  // Type index 0

            // Export section
            0x07,  // Section ID
            0x05,  // Section size
            0x01,  // 1 export
            0x01, 0x66,  // Name length=1, name="f"
            0x00,  // Export kind: function
            0x00,  // Function index 0

            // Code section
            0x0A,  // Section ID
            ...codeSectionSize,
            0x01,  // 1 function body
            ...encodeVarUInt32(codeBody.length),
            ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 42);
    }

    // Test ref.i31 with 2-byte redundant encoding
    {
        let extendedOp = createRedundantLEB128(0x1C, 2);  // ref.i31 redundant: [0x9C, 0x00]
        let codeBody = [
            0x00,  // 0 locals
            0x41, 0x2A,  // i32.const 42
            0xFB, ...extendedOp,  // ref.i31 with redundant encoding
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            // WASM header
            0x00, 0x61, 0x73, 0x6D,  // Magic
            0x01, 0x00, 0x00, 0x00,  // Version

            // Type section [type 0: () -> i31ref]
            0x01,  // Section ID
            0x05,  // Section size
            0x01,  // 1 type
            0x60,  // Function type
            0x00,  // 0 params
            0x01,  // 1 return
            0x6C,  // i31ref (ref null i31)

            // Function section [function 0 uses type 0]
            0x03,  // Section ID
            0x02,  // Section size
            0x01,  // 1 function
            0x00,  // Type index 0

            // Export section
            0x07,  // Section ID
            0x05,  // Section size
            0x01,  // 1 export
            0x01, 0x66,  // Name length=1, name="f"
            0x00,  // Export kind: function
            0x00,  // Function index 0

            // Code section
            0x0A,  // Section ID
            ...codeSectionSize,
            0x01,  // 1 function body
            ...encodeVarUInt32(codeBody.length),
            ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 42);
    }

    // Test ref.i31 with 3-byte redundant encoding
    {
        let extendedOp = createRedundantLEB128(0x1C, 3);  // ref.i31 redundant: [0x9C, 0x80, 0x00]
        let codeBody = [
            0x00,  // 0 locals
            0x41, 0x2A,  // i32.const 42
            0xFB, ...extendedOp,  // ref.i31 with extra redundant encoding
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            // WASM header
            0x00, 0x61, 0x73, 0x6D,  // Magic
            0x01, 0x00, 0x00, 0x00,  // Version

            // Type section [type 0: () -> i31ref]
            0x01,  // Section ID
            0x05,  // Section size
            0x01,  // 1 type
            0x60,  // Function type
            0x00,  // 0 params
            0x01,  // 1 return
            0x6C,  // i31ref (ref null i31)

            // Function section [function 0 uses type 0]
            0x03,  // Section ID
            0x02,  // Section size
            0x01,  // 1 function
            0x00,  // Type index 0

            // Export section
            0x07,  // Section ID
            0x05,  // Section size
            0x01,  // 1 export
            0x01, 0x66,  // Name length=1, name="f"
            0x00,  // Export kind: function
            0x00,  // Function index 0

            // Code section
            0x0A,  // Section ID
            ...codeSectionSize,
            0x01,  // 1 function body
            ...encodeVarUInt32(codeBody.length),
            ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 42);
    }
}

// ========================================
// i31.get_s and i31.get_u Tests
// ========================================

function testI31GetRedundantEncoding() {
    // Test i31.get_s with 2-byte redundant encoding
    {
        let extendedOp1 = [0x1C];  // ref.i31 normal
        let extendedOp2 = createRedundantLEB128(0x1D, 2);  // i31.get_s redundant
        let codeBody = [
            0x00,  // 0 locals
            0x41, 0x2A,  // i32.const 42
            0xFB, ...extendedOp1,  // ref.i31
            0xFB, ...extendedOp2,  // i31.get_s with redundant encoding
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,
            0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7F,  // Type: () -> i32
            0x03, 0x02, 0x01, 0x00,
            0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
            0x0A, ...codeSectionSize, 0x01, ...encodeVarUInt32(codeBody.length), ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 42);
    }

    // Test i31.get_u with 3-byte redundant encoding
    {
        let extendedOp1 = [0x1C];  // ref.i31 normal
        let extendedOp2 = createRedundantLEB128(0x1E, 3);  // i31.get_u redundant
        let codeBody = [
            0x00,  // 0 locals
            0x41, 0x2A,  // i32.const 42
            0xFB, ...extendedOp1,  // ref.i31
            0xFB, ...extendedOp2,  // i31.get_u with extra redundant encoding
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,
            0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7F,  // Type: () -> i32
            0x03, 0x02, 0x01, 0x00,
            0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
            0x0A, ...codeSectionSize, 0x01, ...encodeVarUInt32(codeBody.length), ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 42);
    }
}

// Run tests
testRefI31RedundantEncoding();
testI31GetRedundantEncoding();

// ========================================
// array.len Tests
// ========================================

function testArrayLenRedundantEncoding() {
    // Test array.len with 2-byte redundant encoding
    {
        let extendedOp1 = [0x07];  // array.new_default normal
        let extendedOp2 = createRedundantLEB128(0x0F, 2);  // array.len redundant
        let codeBody = [
            0x00,  // 0 locals
            0x41, 0x05,  // i32.const 5
            0xFB, ...extendedOp1, 0x00,  // array.new_default type_index=0
            0xFB, ...extendedOp2,  // array.len with redundant encoding
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,

            // Type section: 2 types
            0x01, 0x08, 0x02,
            0x5E, 0x7F, 0x01,  // Type 0: (array i32 mutable)
            0x60, 0x00, 0x01, 0x7F,  // Type 1: () -> i32

            // Function section
            0x03, 0x02, 0x01, 0x01,

            // Export section
            0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,

            // Code section
            0x0A, ...codeSectionSize, 0x01, ...encodeVarUInt32(codeBody.length), ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 5);
    }

    // Test array.len with 3-byte redundant encoding
    {
        let extendedOp1 = [0x07];  // array.new_default normal
        let extendedOp2 = createRedundantLEB128(0x0F, 3);  // array.len extra redundant
        let codeBody = [
            0x00,  // 0 locals
            0x41, 0x0A,  // i32.const 10
            0xFB, ...extendedOp1, 0x00,  // array.new_default type_index=0
            0xFB, ...extendedOp2,  // array.len with extra redundant encoding
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,

            // Type section: 2 types
            0x01, 0x08, 0x02,
            0x5E, 0x7F, 0x01,  // Type 0: (array i32 mutable)
            0x60, 0x00, 0x01, 0x7F,  // Type 1: () -> i32

            // Function section
            0x03, 0x02, 0x01, 0x01,

            // Export section
            0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,

            // Code section
            0x0A, ...codeSectionSize, 0x01, ...encodeVarUInt32(codeBody.length), ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 10);
    }
}

// ========================================
// array.fill Tests
// ========================================

function testArrayFillRedundantEncoding() {
    // Test array.fill with 2-byte redundant encoding
    {
        let extendedOp1 = [0x07];  // array.new_default normal
        let extendedOp2 = createRedundantLEB128(0x10, 2);  // array.fill redundant
        let extendedOp3 = [0x0B];  // array.get normal
        let codeBody = [
            0x01, 0x01, 0x63, 0x00,  // 1 local: (ref null 0) = arrayref
            // Create array
            0x41, 0x05,  // i32.const 5
            0xFB, ...extendedOp1, 0x00,  // array.new_default type_index=0
            0x21, 0x00,  // local.set 0
            // Fill array
            0x20, 0x00,  // local.get 0
            0x41, 0x00,  // i32.const 0 (start index)
            0x41, 0x2A,  // i32.const 42 (value)
            0x41, 0x05,  // i32.const 5 (count)
            0xFB, ...extendedOp2, 0x00,  // array.fill with redundant encoding, type_index=0
            // Get and return first element
            0x20, 0x00,  // local.get 0
            0x41, 0x00,  // i32.const 0
            0xFB, ...extendedOp3, 0x00,  // array.get type_index=0
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,

            // Type section: 2 types
            0x01, 0x08, 0x02,
            0x5E, 0x7F, 0x01,  // Type 0: (array i32 mutable)
            0x60, 0x00, 0x01, 0x7F,  // Type 1: () -> i32

            // Function section
            0x03, 0x02, 0x01, 0x01,

            // Export section
            0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,

            // Code section
            0x0A, ...codeSectionSize, 0x01, ...encodeVarUInt32(codeBody.length), ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 42);
    }
}

// ========================================
// array.copy Tests
// ========================================

function testArrayCopyRedundantEncoding() {
    // Test array.copy with 2-byte redundant encoding
    {
        let extendedOp1 = [0x06];  // array.new normal
        let extendedOp2 = [0x07];  // array.new_default normal
        let extendedOp3 = createRedundantLEB128(0x11, 2);  // array.copy redundant
        let extendedOp4 = [0x0B];  // array.get normal
        let codeBody = [
            0x01, 0x02, 0x63, 0x00,  // 1 local declaration: 2 locals of type (ref null 0) = arrayref
            // Create source array with value 99
            0x41, 0xE3, 0x00,  // i32.const 99 (LEB128 encoded)
            0x41, 0x03,  // i32.const 3 (size)
            0xFB, ...extendedOp1, 0x00,  // array.new type_index=0
            0x21, 0x00,  // local.set 0 (source array)
            // Create destination array
            0x41, 0x03,  // i32.const 3
            0xFB, ...extendedOp2, 0x00,  // array.new_default type_index=0
            0x21, 0x01,  // local.set 1 (dest array)
            // Copy from source to dest
            0x20, 0x01,  // local.get 1 (dest)
            0x41, 0x00,  // i32.const 0 (dest index)
            0x20, 0x00,  // local.get 0 (source)
            0x41, 0x00,  // i32.const 0 (source index)
            0x41, 0x03,  // i32.const 3 (count)
            0xFB, ...extendedOp3, 0x00, 0x00,  // array.copy with redundant encoding, dest_type=0, src_type=0
            // Get and return first element of dest
            0x20, 0x01,  // local.get 1
            0x41, 0x00,  // i32.const 0
            0xFB, ...extendedOp4, 0x00,  // array.get type_index=0
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,

            // Type section: 2 types
            0x01, 0x08, 0x02,
            0x5E, 0x7F, 0x01,  // Type 0: (array i32 mutable)
            0x60, 0x00, 0x01, 0x7F,  // Type 1: () -> i32

            // Function section
            0x03, 0x02, 0x01, 0x01,

            // Export section
            0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,

            // Code section
            0x0A, ...codeSectionSize, 0x01, ...encodeVarUInt32(codeBody.length), ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 99);
    }
}

// ========================================
// any.convert_extern and extern.convert_any Tests
// ========================================

function testExternAnyConvertRedundantEncoding() {
    // Test extern.convert_any with 2-byte redundant encoding
    {
        let extendedOp1 = [0x1C];  // ref.i31 normal
        let extendedOp2 = createRedundantLEB128(0x1B, 2);  // extern.convert_any redundant
        let codeBody = [
            0x00,  // 0 locals
            0x41, 0x2A,  // i32.const 42
            0xFB, ...extendedOp1,  // ref.i31
            0xFB, ...extendedOp2,  // extern.convert_any with redundant encoding
            0xD1,  // ref.is_null
            0x45,  // i32.eqz (invert: return 1 if not null, 0 if null)
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,

            // Type section
            0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7F,  // Type: () -> i32

            // Function section
            0x03, 0x02, 0x01, 0x00,

            // Export section
            0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,

            // Code section
            0x0A, ...codeSectionSize, 0x01, ...encodeVarUInt32(codeBody.length), ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 1);  // Should return 1 (not null)
    }

    // Test any.convert_extern with 2-byte redundant encoding
    {
        let extendedOp1 = [0x1C];  // ref.i31 normal
        let extendedOp2 = [0x1B];  // extern.convert_any normal
        let extendedOp3 = createRedundantLEB128(0x1A, 2);  // any.convert_extern redundant
        let codeBody = [
            0x00,  // 0 locals
            0x41, 0x2A,  // i32.const 42
            0xFB, ...extendedOp1,  // ref.i31
            0xFB, ...extendedOp2,  // extern.convert_any
            0xFB, ...extendedOp3,  // any.convert_extern with redundant encoding
            0xD1,  // ref.is_null
            0x45,  // i32.eqz (invert: return 1 if not null, 0 if null)
            0x0B  // end
        ];

        let codeSectionSize = encodeVarUInt32(1 + encodeVarUInt32(codeBody.length).length + codeBody.length);

        let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,

            // Type section
            0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7F,  // Type: () -> i32

            // Function section
            0x03, 0x02, 0x01, 0x00,

            // Export section
            0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,

            // Code section
            0x0A, ...codeSectionSize, 0x01, ...encodeVarUInt32(codeBody.length), ...codeBody
        ]);

        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        assert.eq(instance.exports.f(), 1);  // Should return 1 (not null)
    }
}

// Run tests
testArrayLenRedundantEncoding();
testArrayFillRedundantEncoding();
testArrayCopyRedundantEncoding();
testExternAnyConvertRedundantEncoding();
