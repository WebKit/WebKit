// Regression test for https://bugs.webkit.org/show_bug.cgi?id=318710: streaming
// compilation must resolve function names from the WASM "name" section in stack
// traces, like non-streaming does. Uses the same module and expected frames as the
// non-streaming wasm/function-tests/nameSection.js.

import * as assert from '../assert.js';

let stacktrace;

function makeImportObject() {
    return {
        env: {
            _silly: (i) => { stacktrace = (new Error).stack; return i + 42; },
            memory: new WebAssembly.Memory({ initial: 256, maximum: 256 }),
            DYNAMICTOP_PTR: 0,
            STACKTOP: 0,
            STACK_MAX: 0,
            abort: function () { },
            enlargeMemory: function () { },
            getTotalMemory: function () { },
            abortOnCannotGrowMemory: function () { },
            _emscripten_memcpy_big: function () { },
            ___lock: function () { },
            _abort: function () { },
            ___setErrNo: function () { },
            ___syscall6: function () { },
            ___syscall140: function () { },
            ___syscall146: function () { },
            ___syscall54: function () { },
            ___unlock: function () { },
            table: new WebAssembly.Table({ element: 'funcref', initial: 6, maximum: 6 }),
            memoryBase: 0,
            tableBase: 0,
        }
    };
}

async function main() {
    let wasmBuffer = readFile("./nameSection.wasm", "binary");

    // Small chunks so function bodies compile before the trailing name section is parsed.
    let step = 10;
    let result = await $vm.createWasmStreamingCompilerForInstantiate(function (compiler) {
        for (let i = 0; i < wasmBuffer.byteLength; i += step)
            compiler.addBytes(wasmBuffer.subarray(i, i + Math.min(step, wasmBuffer.byteLength - i)));
    }, makeImportObject());

    let value = result.instance.exports._parrot(1);
    assert.eq(value, 1 + 42);

    assert.isString(stacktrace);
    let lines = stacktrace.split("\n");
    assert.eq(lines[1], "_eggs@wasm-function[21]");
    assert.eq(lines[2], "_bacon@wasm-function[22]");
    assert.eq(lines[3], "_spam@wasm-function[23]");
    assert.eq(lines[4], "_parrot@wasm-function[24]");
}

main().catch(function (error) {
    print(String(error));
    print(String(error.stack));
    $vm.abort();
});
