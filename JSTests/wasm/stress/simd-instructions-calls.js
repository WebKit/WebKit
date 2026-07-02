//@ requireOptions("--useWasmSIMD=1", "--useWasmTailCalls=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const verbose = false;

function logV(...args) {
    if (verbose)
        console.log(...args);
}

const testCases = [
    {
        name: "simple_swap_v128",
        signature: {
            params: ['v128', 'v128'],
            results: ['v128', 'v128']
        },
        // Maps result index → parameter index (swap the two parameters)
        resultMapping: [1, 0]  // result[0] = param[1], result[1] = param[0] (swapped)
    },
    {
        name: "simple_10_results_stack_v128_v128",
        signature: {
            params: ['v128', 'f64'],
            results: ['v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'v128']
        },
        resultMapping: [0, 1, 0, 1, 0, 1, 0, 1, 0, 0]
    },
    {
        name: "simple_10_results_stack_f64_f64",
        signature: {
            params: ['v128', 'f64'],
            results: ['v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'f64', 'f64']
        },
        resultMapping: [0, 1, 0, 1, 0, 1, 0, 1, 1, 1]
    },
    {
        name: "simple_10_results_stack_f64_v128",
        signature: {
            params: ['v128', 'f64'],
            results: ['v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'f64', 'v128']
        },
        resultMapping: [0, 1, 0, 1, 0, 1, 0, 1, 1, 0]
    },
        {
        name: "simple_10_results_stack_v128_f64",
        signature: {
            params: ['v128', 'f64'],
            results: ['v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64']
        },
        resultMapping: [0, 1, 0, 1, 0, 1, 0, 1, 0, 1]
    },
        {
        name: "simple_20_results_stack_i64_v128_i64_v128",
        signature: {
            params: ['i64', 'f64'],
            results: ['i64', 'f64', 'i64', 'f64', 'i64', 'f64', 'i64', 'f64', 'i64', 'f64', 'i64', 'f64', 'i64', 'f64', 'i64', 'f64', 'i64', 'f64', 'i64', 'f64'] // 10 results
        },
        resultMapping: [0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1]
    },
    {
        name: "many_args_alternating_f64_v128",
        signature: {
            params: ['f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'i32'],
            results: ['v128']
        },
        resultMapping: [11]
    },
    {
        name: "many_args_alternating_v128_f64",
        signature: {
            params: ['v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128'],
            results: ['v128']
        },
        resultMapping: [10]
    },
    {
        name: "many_args_alternating_many_results_reversed",
        signature: {
            params: ['v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64'],
            results: ['f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128'],
        },
        resultMapping: [11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0]
    },
    {
        name: "many_args_alternating_many_results_shifted",
        signature: {
            params: ['v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64'],
            results: ['v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64'],
        },
        resultMapping: [2, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3]
    },
    {
        name: "stack_args_no_stack_returns",
        signature: {
            params: ['f64', 'v128', 'f64', 'v128', 'v128', 'f64', 'f64', 'v128', 'f64', 'f64', 'v128', 'f64', 'v128'],
            results: ['f64', 'f64', 'f64', 'f64', 'v128', 'v128', 'v128']
        },
        resultMapping: [8, 5, 9, 11, 12, 3, 10]
    },
    {
        name: "no_stack_args_with_stack_returns",
        signature: {
            params: ['f64', 'f64', 'f64', 'f64', 'v128', 'v128', 'v128'],
            results: ['f64', 'v128', 'f64', 'v128', 'v128', 'f64', 'f64', 'v128', 'f64', 'f64', 'v128', 'f64', 'v128']
        },
        resultMapping: [1, 6, 0, 4, 5, 1, 2, 6, 3, 0, 4, 1, 5]
    },
    {
        name: "pure_v128_many_results",
        signature: {
            params: ['v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128'],
            results: ['v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128']
        },
        resultMapping: [0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3]
    },
    {
        name: "odd_results_v128_f64",
        signature: {
            params: ['v128', 'v128', 'v128', 'v128', 'v128', 'f64', 'f64', 'f64', 'f64'],
            results: ['v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128'] // 9 results (odd)
        },
        resultMapping: [0, 5, 1, 6, 2, 7, 3, 8, 4]
    },
    {
        name: "alternating_25_results",
        signature: {
            params: ['v128', 'v128', 'v128', 'v128', 'v128', 'f64', 'f64', 'f64', 'f64'],
            results: new Array(25).fill(null).map((_, i) => i % 2 === 0 ? 'v128' : 'f64')
        },
        resultMapping: new Array(25).fill(null).map((_, i) => i % 2 === 0 ? (i % 10) % 5 : 5 + (i % 8) % 4)
    },
    {
        name: "v128_20_results",
        signature: {
            params: ['v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128', 'v128'], // 9 v128 params
            results: new Array(20).fill('v128') // 20 v128 results
        },
        resultMapping: new Array(20).fill(null).map((_, i) => i % 9)
    },
    {
        name: "random_case_1",
        signature: {
            params: ['f64', 'v128', 'i64', 'f64', 'v128', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64'], // 14 params
            results: ['v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64'] // 18 results
        },
        resultMapping: [1, 0, 2, 4, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 8, 4, 7, 11]
    },
    {
        name: "random_case_2",
        signature: {
            params: ['v128', 'i64', 'v128', 'f64', 'v128', 'i64', 'f64', 'v128', 'i64', 'f64', 'v128', 'i64'], // 12 params
            results: ['i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64'] // 21 results
        },
        resultMapping: [1, 0, 3, 5, 2, 6, 8, 4, 9, 11, 7, 3, 1, 0, 6, 5, 2, 9, 8, 4, 3]
    },
        {
        name: "random_case_3",
        signature: {
            params: ['f64', 'f64', 'v128', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64', 'v128', 'f64', 'i64'], // 16 params
            results: ['v128', 'i64', 'f64', 'v128', 'i64', 'f64', 'v128', 'i64', 'f64', 'v128', 'i64', 'f64', 'v128'] // 13 results
        },
        resultMapping: [2, 3, 0, 4, 6, 1, 7, 9, 8, 10, 12, 11, 13]
    },
]

function generateSignature(name, params, results) {
    const paramStr = params.map((type, i) => `(param $p${i} ${type})`).join(' ');
    const resultStr = results.map(type => `(result ${type})`).join(' ');
    return `(func $${name} ${paramStr} ${resultStr}`;
}

function generateCalleeBody(resultMapping) {
    return resultMapping.map(paramIndex => `(local.get $p${paramIndex})`).join('\n        ');
}

function generateCallerBody(callOp, testCase) {
    let body = '';

    const inputs = generateInputs(testCase.signature.params);

    for (let i = 0; i < inputs.length; i++) {
        const input = inputs[i];
        const type = testCase.signature.params[i];

        if (Array.isArray(input)) {
            body += `        (v128.const i32x4 0x${input[0].toString(16)} 0x${input[1].toString(16)} 0x${input[2].toString(16)} 0x${input[3].toString(16)})\n`;
        } else {
            const value = (type === 'i32' || type === 'i64') ? Math.floor(input) : input;
            body += `        (${type}.const ${value})\n`;
        }
    }

    body += `        (${callOp} $${testCase.name}_callee)`;
    return body;
}

function generateInputs(params) {
    return params.map((type, i) => {
        if (type === 'v128') {
            const base = 0x1000 + i * 0x100;
            return [base, base + 0x10, base + 0x20, base + 0x30];
        } else if (type === 'f64' || type === 'f32') {
            return 10.5 + i;
        } else if (type === 'i32' || type === 'i64') {
            return 1000 + i;
        }
    });
}

function buildWAT(callOp, testCases) {
    let functions = '';
    let exports = '';

    for (const testCase of testCases) {
        const calleeName = `${testCase.name}_callee`;
        const callerName = `${testCase.name}_caller`;

        // Generate callee function
        functions += generateSignature(calleeName, testCase.signature.params, testCase.signature.results) + '\n';
        functions += '        ' + generateCalleeBody(testCase.resultMapping) + '\n';
        functions += '    )\n\n';

        // Generate caller function (same result signature as callee)
        functions += `    (func $${callerName} ${testCase.signature.results.map(type => `(result ${type})`).join(' ')}\n`;
        functions += generateCallerBody(callOp, testCase) + '\n';
        functions += '    )\n\n';

        // Generate export wrapper (just calls caller and stores results)
        exports += `    (func (export "${testCase.name}") (param $result_addr i32)\n`;

        // Add local declarations for temporary storage
        for (let i = 0; i < testCase.signature.results.length; i++) {
            const type = testCase.signature.results[i];
            exports += `        (local $temp${i} ${type})\n`;
        }

        exports += `        (call $${callerName})\n`;

        // Pop results from stack in reverse order into locals
        for (let i = testCase.signature.results.length - 1; i >= 0; i--) {
            exports += `        (local.set $temp${i})\n`;
        }

        // Store results to memory with 16-byte spacing, in forward order
        for (let i = 0; i < testCase.signature.results.length; i++) {
            const offset = i * 16;
            const type = testCase.signature.results[i];
            exports += `        (${type}.store offset=${offset} (local.get $result_addr) (local.get $temp${i}))\n`;
        }
        exports += '    )\n\n';
    }

    return `
(module
    (memory (export "memory") 1)

${functions}
${exports}
)
`;
}

async function runTests(callOp, testCases) {
    const wat = buildWAT(callOp, testCases);
    logV(`\n=== Generated WAT for ${callOp} ===`);
    logV(wat);
    logV('=== End WAT ===\n');

    const instance = await instantiate(wat, {}, { simd: true, tail_call: true });
    const { memory } = instance.exports;

    const f64View = new Float64Array(memory.buffer);
    const i32View = new Int32Array(memory.buffer);
    const i64View = new BigInt64Array(memory.buffer);

    function getI32x4(byteOffset) {
        const i32Offset = byteOffset / 4;
        return [i32View[i32Offset], i32View[i32Offset + 1], i32View[i32Offset + 2], i32View[i32Offset + 3]];
    }

    function getF64(byteOffset) {
        return f64View[byteOffset / 8];
    }

    function getI64(byteOffset) {
        return i64View[byteOffset / 8];
    }

    function verifyTestCase(testCase, resultAddr, expectedInputs) {
        // Verify results match expected values based on result mapping
        // Results are stored at 16-byte intervals: result[0] at offset 0, result[1] at offset 16, etc.
        for (let i = 0; i < testCase.signature.results.length; i++) {
            const offset = i * 16;
            const type = testCase.signature.results[i];
            const mappedParamIndex = testCase.resultMapping[i];
            const expectedValue = expectedInputs[mappedParamIndex];

            if (type === 'f64') {
                const actual = getF64(offset);
                assert.eq(actual, expectedValue, `Result ${i} (f64) should be ${expectedValue}, got ${actual}`);
                logV(`    Result ${i} (f64): ${actual} ✓`);
            } else if (type === 'i64') {
                const actual = getI64(offset);
                const expectedBigInt = BigInt(expectedValue);
                assert.eq(actual, expectedBigInt, `Result ${i} (i64) should be ${expectedBigInt}, got ${actual}`);
                logV(`    Result ${i} (i64): ${actual} ✓`);
            } else if (type === 'v128') {
                const actual = getI32x4(offset);
                for (let j = 0; j < 4; j++) {
                    assert.eq(actual[j], expectedValue[j], `Result ${i} (v128) lane ${j} should be 0x${expectedValue[j].toString(16)}, got 0x${actual[j].toString(16)}`);
                }
                logV(`    Result ${i} (v128): [0x${actual.map(x => x.toString(16)).join(', 0x')}] ✓`);
            }
        }
    }

    logV(`Testing with ${callOp}...`);

    for (const testCase of testCases) {
        logV(`  Running ${testCase.name}...`);
        const resultAddr = 0;
        const expectedInputs = generateInputs(testCase.signature.params);

        // Run multiple times to trigger tier-up (100 iterations to ensure BBQ compilation)
        for (let iteration = 0; iteration < wasmTestLoopCount; iteration++) {
            // Clear memory
            for (let i = 0; i < 256; i++)
                i32View[i] = 0;

            // Run the test case and verify
            instance.exports[testCase.name](resultAddr);
            verifyTestCase(testCase, resultAddr, expectedInputs);
        }
    }
}

async function test() {
    const operations = ['call', 'return_call'];

    for (const callOp of operations) {
        await runTests(callOp, testCases);
    }
}

await assert.asyncTest(test())