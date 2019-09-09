asyncTestStart(1);
let context = {
    env: globalThis,
};

globalThis.__linear_memory = new WebAssembly.Memory({ initial: 1 });

async function runWasmFile(filePath) {
    let blob = readFile(filePath, "binary");
    let compiled;
    try {
        compiled = await WebAssembly.instantiate(blob, context);
        compiled.instance.exports.test();
    } catch (e) {
        print(e);
        throw e;
    }
    asyncTestPassed();
}

for (wasmFile of arguments)
    runWasmFile(wasmFile);
