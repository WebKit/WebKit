asyncTestStart(1);
let context = {
    env: globalThis,
};

globalThis.__linear_memory = new WebAssembly.Memory({ initial: 1 });

async function runWasmFile(filePath) {
    let blob = readFile(filePath, "binary");
    let instance;
    let result;
    try {
        let compiled = await WebAssembly.instantiate(blob, context);
        instance = compiled.instance;
        result = instance.exports.test();

        if (instance.exports.checkResult && !instance.exports.checkResult(result))
            throw new Error("Got result " + result + " but checkResult returned false");
    } catch (e) {
        print(e);
        throw e;
    }

    asyncTestPassed();
}

for (wasmFile of arguments)
    runWasmFile(wasmFile);
