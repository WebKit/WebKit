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
        await $vm.createWasmStreamingCompilerForCompile(function (compiler) {
        });
    } catch (error) {
        shouldBe(error instanceof WebAssembly.CompileError, true);
    }

    {
        let wasmBuffer = readFile("./resources/tsf.wasm", "binary");
        await $vm.createWasmStreamingCompilerForCompile(function (compiler) {
            slice(wasmBuffer, 100, (buffer) => compiler.addBytes(buffer));
        });
        await $vm.createWasmStreamingCompilerForCompile(function (compiler) {
            slice(wasmBuffer, 1000, (buffer) => compiler.addBytes(buffer));
        });
    }
}

main().catch(function (error) {
    print(String(error));
    print(String(error.stack));
    $vm.abort()
});
