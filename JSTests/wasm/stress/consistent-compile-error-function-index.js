// Concurrent function validation should report the CompileError for the lowest
// failing function index, not whichever worker finishes first.
// https://bugs.webkit.org/show_bug.cgi?id=283476
//@ requireOptions("--useConcurrentJIT=true")

import * as assert from "../assert.js";

function leb(value) {
    let bytes = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

function section(id, payload) {
    return [id, ...leb(payload.length), ...payload];
}

// Many invalid functions. Function 0 is large (slow to validate) so a higher-index
// worker is likely to finish first under concurrent compilation. The reported
// error must still be for function 0.
function invalidModuleBytes() {
    const types = [0x01, 0x60, 0x00, 0x00];
    const functionCount = 16;
    const functions = [functionCount, ...Array(functionCount).fill(0x00)];

    // Function 0: many nops then empty-stack i32.add (fails late).
    const body0 = [0x00];
    for (let i = 0; i < 8000; ++i)
        body0.push(0x01); // nop
    body0.push(0x6a, 0x0b); // i32.add end

    // Functions 1..N: immediate empty-stack f32.add (fails quickly).
    const bodyFast = [0x00, 0x92, 0x0b];

    const codePayload = [functionCount, ...leb(body0.length), ...body0];
    for (let i = 1; i < functionCount; ++i)
        codePayload.push(...leb(bodyFast.length), ...bodyFast);

    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...section(1, types),
        ...section(3, functions),
        ...section(10, codePayload),
    ]);
}

const bytes = invalidModuleBytes();
const iterations = 80;

for (let i = 0; i < iterations; ++i) {
    let message;
    try {
        new WebAssembly.Module(bytes);
        throw new Error("expected CompileError");
    } catch (error) {
        assert.truthy(error instanceof WebAssembly.CompileError, `expected CompileError, got ${error}`);
        message = String(error);
    }
    assert.truthy(
        message.includes("function at index 0"),
        `iteration ${i}: expected error for function 0, got: ${message}`
    );
    for (let j = 1; j < 16; ++j) {
        assert.truthy(
            !message.includes(`function at index ${j}`),
            `iteration ${i}: should not report function ${j} when function 0 also fails: ${message}`
        );
    }
}
