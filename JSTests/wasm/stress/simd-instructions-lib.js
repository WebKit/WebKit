//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

/**
 * Convert a floating point value to WebAssembly text format
 * @param {number} val - JavaScript number value
 * @returns {string} - WebAssembly text format representation
 */
function floatToWasmText(val) {
    if (val === Number.POSITIVE_INFINITY) return 'inf';
    if (val === Number.NEGATIVE_INFINITY) return '-inf';
    if (Number.isNaN(val)) return 'nan';
    // Handle signed zero: -0.0 should be represented as "-0.0" in WebAssembly text
    if (val === 0 && 1/val === -Infinity) return '-0.0';
    return val.toString();
}

/**
 * Get input vector type for an instruction by parsing the instruction name
 * @param {string} instruction - SIMD instruction name
 * @returns {string} - Input vector type (e.g., 'f64x2', 'f32x4', 'i32x4')
 */
function getInputVectorType(instruction) {
    // Look for vector type pattern in the instruction name (after the operation)
    const vectorTypeMatch = instruction.match(/\.(?:\w+_)*([if]\d+x\d+)(?:_[su])?(?:_zero)?$/);
    if (vectorTypeMatch)
        return vectorTypeMatch[1];
    
    // Default: input type is same as output type (extract from instruction prefix)
    const prefixMatch = instruction.match(/^(v128|[if]\d+x\d+)\./);
    if (prefixMatch) {
        // v128 instruction inputs expressed as i8x16
        return prefixMatch[1] === 'v128' ? 'i8x16' : prefixMatch[1];
    }

    return 'unknown';
}

/**
 * Convert array to v128.const string based on vector type
 * @param {Array} array - Input array
 * @param {string} vectorType - Vector type (e.g., 'f32x4', 'f64x2', 'i32x4')
 * @returns {string} - v128.const string
 */
function arrayToV128ConstByType(array, vectorType) {
    if (vectorType === 'i8x16') {
        const hexValues = array.map(val => {
            // Convert to unsigned 8-bit
            const unsigned = (val & 0xFF) >>> 0;
            return `0x${unsigned.toString(16).padStart(2, '0').toUpperCase()}`;
        });
        return `(v128.const i8x16 ${hexValues.join(' ')})`;
    } else if (vectorType === 'i16x8') {
        const hexValues = array.map(val => {
            // Convert to unsigned 16-bit
            const unsigned = (val & 0xFFFF) >>> 0;
            return `0x${unsigned.toString(16).padStart(4, '0').toUpperCase()}`;
        });
        return `(v128.const i16x8 ${hexValues.join(' ')})`;
    } else if (vectorType === 'i32x4') {
        const hexValues = array.map(val => {
            // Convert to unsigned 32-bit
            const unsigned = val >>> 0;
            return `0x${unsigned.toString(16).padStart(8, '0').toUpperCase()}`;
        });
        return `(v128.const i32x4 ${hexValues.join(' ')})`;
    } else if (vectorType === 'i64x2') {
        const hexValues = array.map(val => {
            let bigIntVal = typeof val === 'bigint' ? val : BigInt(val);
            // Convert to unsigned 64-bit using BigInt mask
            const unsigned = bigIntVal & 0xFFFFFFFFFFFFFFFFn;
            return `0x${unsigned.toString(16).padStart(16, '0').toUpperCase()}`;
        });
        return `(v128.const i64x2 ${hexValues.join(' ')})`;
    } else if (vectorType === 'f32x4') {
        const wasmValues = array.map(floatToWasmText);
        return `(v128.const f32x4 ${wasmValues.join(' ')})`;
    } else if (vectorType === 'f64x2') {
        const wasmValues = array.map(floatToWasmText);
        return `(v128.const f64x2 ${wasmValues.join(' ')})`;
    }
    // Default fallback - assume it's already a string
    return array;
}

/**
 * Convert array to v128.const string based on instruction type
 * @param {Array} array - Input array
 * @param {string} instruction - SIMD instruction name
 * @returns {string} - v128.const string
 */
function arrayToV128Const(array, instruction) {
    const inputType = getInputVectorType(instruction);
    return arrayToV128ConstByType(array, inputType);
}

/**
 * Convert scalar value to WebAssembly text format based on instruction type
 * @param {*} val - Scalar value
 * @param {string} instruction - SIMD instruction name
 * @returns {string} - WebAssembly text format representation
 */
function scalarToWasmText(val, instruction) {
    if (instruction.startsWith('i8x16.') || instruction.startsWith('i16x8.') || instruction.startsWith('i32x4.')) {
        // Integer splat instructions take i32 constants
        return `(i32.const ${val})`;
    } else if (instruction.startsWith('i64x2.')) {
        // i64 splat instruction takes i64 constant
        const bigIntVal = typeof val === 'bigint' ? val : BigInt(val);
        return `(i64.const ${bigIntVal})`;
    } else if (instruction.startsWith('f32x4.')) {
        // f32 splat instruction takes f32 constant
        return `(f32.const ${floatToWasmText(val)})`;
    } else if (instruction.startsWith('f64x2.')) {
        // f64 splat instruction takes f64 constant
        return `(f64.const ${floatToWasmText(val)})`;
    }
    // Default fallback
    return val.toString();
}

/**
 * Run SIMD instruction tests with given test data
 * @param {Array} testData - Array of test cases, each containing [instruction, input0, input1, expected]
 * @param {boolean} verbose - Whether to print verbose output
 * @param {string} testType - Description of test type for logging
 */
export async function runSIMDTests(testData, verbose = false, testType = "SIMD") {

    const numInputs = instruction => {
        if (/\.(bitselect|shuffle|replace_lane)/.test(instruction)) return 3;
        
        if (/\.(abs|neg|sqrt|not|any_true|popcnt|all_true|bitmask|splat|ceil|floor|trunc|nearest)/.test(instruction)) return 1;
        if (/\.(demote|promote|convert|extend|extadd_pairwise)/.test(instruction)) return 1;
        
        // Default: 2-input instructions (binary operations)
        return 2;
    };

    const returnsI32 = instruction => ['.any_true', '.all_true', '.bitmask'].some(pattern => instruction.includes(pattern));

    // Generate WebAssembly module
    let wat = `
(module
    (memory (export "memory") 1)
`;

    testData.forEach((test, index) => {
        const [instruction, arg0, arg1, arg2, arg3] = test;

        const numArgs = numInputs(instruction);

        const input0Str = Array.isArray(arg0) ? arrayToV128Const(arg0, instruction) :
                         (instruction.includes('.splat') ? scalarToWasmText(arg0, instruction) : arg0);
        
        if (returnsI32(instruction)) {
            // Instructions that return i32 (like v128.any_true) store to i32 memory location
            wat += `
    (func (export "test_${index}") (param $addr i32)
        (i32.store (local.get $addr)
`;
        } else {
            // Instructions that return v128 store to v128 memory location
            wat += `
    (func (export "test_${index}") (param $addr i32)
        (v128.store (local.get $addr)
`;
        }
        
        if (numArgs === 1) {
            wat += `            (${instruction} ${input0Str})`;
        } else if (numArgs === 2) {
            const input1Str = Array.isArray(arg1) ? arrayToV128Const(arg1, instruction) : arg1;
            wat += `            (${instruction} ${input0Str} ${input1Str})`;
        } else if (numArgs === 3) {
            const input1Str = Array.isArray(arg1) ? arrayToV128Const(arg1, instruction) : arg1;
            if (instruction.includes('.shuffle')) {
                // For shuffle, arg2 contains the 16 immediate indices that come right after instruction name
                const indices = arg2.join(' ');
                wat += `            (${instruction} ${indices} ${input0Str} ${input1Str})`;
            } else if (instruction.includes('.replace_lane')) {
                // For replace_lane, arg2 is the lane index (immediate), arg1 is the replacement value
                const laneIndex = arg2;
                const replacementStr = scalarToWasmText(arg1, instruction);
                wat += `            (${instruction} ${laneIndex} ${input0Str} ${replacementStr})`;
            } else {
                // For other 3-arg instructions like bitselect
                const input2Str = Array.isArray(arg2) ? arrayToV128Const(arg2, instruction) : arg2;
                wat += `            (${instruction} ${input0Str} ${input1Str} ${input2Str})`;
            }
        } else
            assert.fail(`Unsupported number of arguments: ${numArgs} for instruction: ${instruction}`);

        wat += `)
    )
`;
    });

    wat += `
)
`;

    if (verbose) {
        print("Generated WebAssembly text:");
        print(wat);
    }

    const instance = await instantiate(wat, {}, { simd: true });
    const memory = instance.exports.memory;
    const buffer = memory.buffer;
    const u8 = new Uint8Array(buffer);
    const u16 = new Uint16Array(buffer);
    const u32 = new Uint32Array(buffer);
    const u64 = new BigUint64Array(buffer);
    const f32 = new Float32Array(buffer);
    const f64 = new Float64Array(buffer);

    function clearMemory() {
        u8.fill(0);
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        testData.forEach((test, testIndex) => {
            const [instruction, arg0, arg1, arg2, arg3] = test;
            const numArgs = numInputs(instruction);
            let expected;
            if (numArgs === 1)
                expected = arg1;
            else if (numArgs === 2)
                expected = arg2;
            else if (numArgs === 3) {
                // For all 3-arg instructions (shuffle, bitselect), expected result is arg3
                expected = arg3;
            } else
                assert.fail(`Unsupported number of arguments: ${numArgs} for instruction: ${instruction}`);

            if (verbose)
                print(`Testing ${instruction} test ${testIndex}...`);
            
            clearMemory();
            
            // Call the test function
            const testFunc = instance.exports[`test_${testIndex}`];
            testFunc(0);
            
            // Backtraces for table driven test cases is not helpful, so print test case context on failure.
            function assertEqWithContext(actual, expectedValue, lane, actualArray) {
                try {
                    assert.eq(actual, expectedValue);
                } catch (e) {
                    print(`\n=== TEST CASE FAILURE ===`);
                    print(`Test Index: ${testIndex}`);
                    print(`Instruction: ${instruction}`);
                    print(`Input 0: ${Array.isArray(arg0) ? `[${arg0.join(', ')}]` : arg0}`);
                    if (numArgs >= 2) {
                        print(`Input 1: ${Array.isArray(arg1) ? `[${arg1.join(', ')}]` : arg1}`);
                    }
                    if (numArgs >= 3) {
                        print(`Input 2: ${Array.isArray(arg2) ? `[${arg2.join(', ')}]` : arg2}`);
                    }
                    if (returnsI32(instruction)) {
                        print(`Expected Value: ${expected}`);
                    } else {
                        print(`Expected Array: [${expected.join(', ')}]`);
                    }
                    print(`Actual Array: [${Array.from(actualArray).join(', ')}]`);
                    print(`Lane: ${lane}`);
                    print(`Expected Value: ${expectedValue}`);
                    print(`Actual Value: ${actual}`);
                    print(`========================`);
                    throw e;
                }
            }

            // Verify the result using appropriate data type
            if (returnsI32(instruction)) {
                // Instructions that return i32 (like v128.any_true)
                assertEqWithContext(u32[0], expected, 0, [u32[0]]);
            } else if (instruction.startsWith('i8x16.') || instruction.startsWith('v128.')) {
                for (let j = 0; j < 16; j++)
                    assertEqWithContext(u8[j], expected[j], j, u8.slice(0, 16));
            } else if (instruction.startsWith('i16x8.')) {
                for (let j = 0; j < 8; j++)
                    assertEqWithContext(u16[j], expected[j], j, u16.slice(0, 8));
            } else if (instruction.startsWith('i32x4.') ||
                        (instruction === 'f32x4.eq' || instruction === 'f32x4.ne' ||
                         instruction === 'f32x4.lt' || instruction === 'f32x4.gt' ||
                         instruction === 'f32x4.le' || instruction === 'f32x4.ge')) {
                for (let j = 0; j < 4; j++)
                    assertEqWithContext(u32[j], expected[j], j, u32.slice(0, 4));
            } else if (instruction.startsWith('f32x4.')) {
                for (let j = 0; j < 4; j++)
                    assertEqWithContext(f32[j], expected[j], j, f32.slice(0, 4));
            } else if (instruction.startsWith('i64x2.') ||
                        (instruction === 'f64x2.eq' || instruction === 'f64x2.ne' ||
                         instruction === 'f64x2.lt' || instruction === 'f64x2.gt' ||
                         instruction === 'f64x2.le' || instruction === 'f64x2.ge')) {
                for (let j = 0; j < 2; j++)
                    assertEqWithContext(u64[j], expected[j], j, u64.slice(0, 2));
            } else if (instruction.startsWith('f64x2.')) {
                for (let j = 0; j < 2; j++)
                    assertEqWithContext(f64[j], expected[j], j, f64.slice(0, 2));
            } else
                assert.fail(`Unhandled instruction format: ${instruction}`);
            
            if (verbose)
                print(`✓ ${instruction} test ${testIndex} passed`);
        });
    }
    
    if (verbose)
        print(`All ${testData.length} ${testType} tests passed!`);
}