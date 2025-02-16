//@ runDefault("--useConcurrentJIT=0")

function main() {
    const wasm_module_code = readFile('./resources/array-element-creation.wasm', 'binary');
    const instance = new WebAssembly.Instance(new WebAssembly.Module(wasm_module_code), {});

    const set = new Set();
    const array = instance.exports.createArray();
    for (let i = 0; i < 500000; i++) {
        set.add(instance.exports.arrayGet(array, i));
    }

    if (set.size !== 500000)
        throw new Error(set.size);
}

main();
