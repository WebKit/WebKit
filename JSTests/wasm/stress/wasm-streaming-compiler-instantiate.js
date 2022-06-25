function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function slice(array, step, func) {
    for (let index = 0; index < array.length; index += step)
        func(array.slice(index, index + step));
}

async function main() {
    try {
        await $vm.createWasmStreamingCompilerForInstantiate(function (compiler) {
        });
    } catch (error) {
        shouldBe(error instanceof WebAssembly.CompileError, true);
    }

    {
        let wasmBuffer = readFile("./nameSection.wasm", "binary");
        await $vm.createWasmStreamingCompilerForInstantiate(function (compiler) {
            slice(wasmBuffer, 10, (buffer) => compiler.addBytes(buffer));
        }, {
            env: {
                memory: new WebAssembly.Memory({ initial: 256, maximum: 256  }),
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
                _silly: function () { },
                ___unlock: function () { },
                table: new WebAssembly.Table({ element: 'funcref', initial: 6, maximum: 6 }),
                memoryBase: 0,
                tableBase: 0,
            }
        });
        await $vm.createWasmStreamingCompilerForInstantiate(function (compiler) {
            slice(wasmBuffer, 20, (buffer) => compiler.addBytes(buffer));
        }, {
            env: {
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
                _silly: function () { },
                ___unlock: function () { },
                table: new WebAssembly.Table({ element: 'funcref', initial: 6, maximum: 6 }),
                memoryBase: 0,
                tableBase: 0,
            }
        });
    }
}

main().catch(function (error) {
    print(String(error));
    print(String(error.stack));
    $vm.abort()
});
