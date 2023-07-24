import * as assert from "../assert.js"

async function test() {
    let instances = []
    let insttime = 0;
    let bytes = read('ipint-stress-lol.wasm', 'binary');
    for (let i = 0; i < 1000; ++i) {
        let start = Date.now();
        const instance = new WebAssembly.Module(bytes);
        insttime += (Date.now() - start);
        instances.push(instance);
    }

    const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), {});
    const { million } = instance.exports
    let start = Date.now();
    let x = 0;
    for (let i = 0; i < 1000; ++i) {
        x = million();
    }
    print("Total execution:", Date.now() - start);
    print("Instantiation time:", insttime)
    print("Result", x)
}

assert.asyncTest(test())
