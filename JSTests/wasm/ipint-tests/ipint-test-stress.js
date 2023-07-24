import * as assert from "../assert.js"

async function test() {
    let instances = []
    let insttime = 0;
    let bytes = read('ipint-stress.wasm', 'binary');
    for (let i = 0; i < 50; ++i) {
        let start = Date.now();
        const instance = new WebAssembly.Module(bytes);
        insttime += (Date.now() - start);
        instances.push(instance);
    }

    const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), {});
    const { fib } = instance.exports
    let start = Date.now();
    for (let i = 0; i < 1000; ++i) {
        fib(42);
    }
    print("Total execution:", Date.now() - start);
    print("Instantiation time:", insttime)
}

assert.asyncTest(test())
