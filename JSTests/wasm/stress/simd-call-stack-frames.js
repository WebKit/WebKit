//@ requireOptions("--useWasmSIMD=1", "--useWasmTailCalls=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const verbose = true;

const testCases = [
    {
        name: "swap_v128_simple",
        signature: {
            params: ['v128', 'v128'],
            results: ['v128', 'v128']
        },
        // Maps result index → parameter index (swap the two parameters)
        resultMapping: [1, 0]  // result[0] = param[1], result[1] = param[0] (swapped)
    },
    {
        name: "many_args_alternating",
        signature: {
            params: ['f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'f64', 'v128', 'i32'],
            results: ['v128']
        },
        resultMapping: [11]
    },
    {
        name: "stack_args_no_stack_returns",
        signature: {
            params: ['f64', 'v128', 'f64', 'v128', 'v128', 'f64', 'f64', 'v128', 'f64', 'f64', 'v128', 'f64', 'v128'],
            results: ['f64', 'f64', 'f64', 'f64', 'v128', 'v128', 'v128']
        },
        resultMapping: [8, 5, 9, 11, 12, 3, 10]
    }
    // More test cases can be added here
];

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
            if (type === 'f64') {
                exports += `        (f64.store offset=${offset} (local.get $result_addr) (local.get $temp${i}))\n`;
            } else if (type === 'v128') {
                exports += `        (v128.store offset=${offset} (local.get $result_addr) (local.get $temp${i}))\n`;
            }
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

    if (verbose) {
        console.log(`\n=== Generated WAT for ${callOp} ===`);
        console.log(wat);
        console.log('=== End WAT ===\n');
    }

    const instance = await instantiate(wat, {}, { simd: true, tail_call: true });
    const { memory } = instance.exports;

    const f64View = new Float64Array(memory.buffer);
    const i32View = new Int32Array(memory.buffer);

    function getI32x4(byteOffset) {
        const i32Offset = byteOffset / 4;
        return [i32View[i32Offset], i32View[i32Offset + 1], i32View[i32Offset + 2], i32View[i32Offset + 3]];
    }

    function getF64(byteOffset) {
        return f64View[byteOffset / 8];
    }

    console.log(`Testing with ${callOp}...`);

    for (const testCase of testCases) {
        console.log(`  Running ${testCase.name}...`);
        const resultAddr = 0;

        // Clear memory
        for (let i = 0; i < 256; i++) {
            i32View[i] = 0;
        }

        // Run the test
        instance.exports[testCase.name](resultAddr);

        // Generate expected callee inputs to verify against
        const expectedInputs = generateInputs(testCase.signature.params);

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
                console.log(`    Result ${i} (f64): ${actual} ✓`);
            } else if (type === 'v128') {
                const actual = getI32x4(offset);
                for (let j = 0; j < 4; j++) {
                    assert.eq(actual[j], expectedValue[j], `Result ${i} (v128) lane ${j} should be 0x${expectedValue[j].toString(16)}, got 0x${actual[j].toString(16)}`);
                }
                console.log(`    Result ${i} (v128): [0x${actual.map(x => x.toString(16)).join(', 0x')}] ✓`);
            }
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