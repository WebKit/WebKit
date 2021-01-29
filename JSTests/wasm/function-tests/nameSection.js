import * as assert from '../assert.js'

/*
This test loads a WebAssembly file compiled by Emscripten with:
  ./emsdk-portable/emscripten/incoming/em++ ./nameSection.cc -O2 -g4 -s WASM=1 -o nameSection.js -s EXPORTED_FUNCTIONS="['_parrot']"

From the following C++ source file:
  extern "C" {
  int silly(int);
  __attribute__((noinline)) int eggs(int i) { return silly(i); }
  __attribute__((noinline)) int bacon(int i) { return eggs(i); }
  __attribute__((noinline)) int spam(int i) { return bacon(i); }
  __attribute__((noinline)) int parrot(int i) { return spam(i); }
  }
*/

const verbose = false;
const wasmFile = 'nameSection.wasm';

const compile = (location, importObject = {}) => {
    if (verbose)
        print(`Processing ${location}`);
    let buf = typeof readbuffer !== "undefined"? readbuffer(location) : read(location, 'binary');
    if (verbose)
        print(`  Size: ${buf.byteLength}`);

    let t0 = Date.now();
    let module = new WebAssembly.Module(buf);
    let t1 = Date.now();
    if (verbose)
        print(`new WebAssembly.Module(buf) took ${t1-t0} ms.`);

    if (verbose)
        print(`Creating fake import object with ${WebAssembly.Module.imports(module).length} imports`);
    for (let imp of WebAssembly.Module.imports(module)) {
        if (typeof importObject[imp.module] === "undefined")
            importObject[imp.module] = {};
        if (typeof importObject[imp.module][imp.name] === "undefined") {
            switch (imp.kind) {
            case "function": importObject[imp.module][imp.name] = () => {}; break;
            case "table": importObject[imp.module][imp.name] = new WebAssembly.Table({ initial: 6, maximum: 6, element: "funcref" }); break;
            case "memory": importObject[imp.module][imp.name] = new WebAssembly.Memory({ initial: 16777216 / (64 * 1024), maximum: 16777216 / (64 * 1024) }); break;
            case "global": importObject[imp.module][imp.name] = 0; break;
            }
        }

    }

    let t2 = Date.now();
    let instance = new WebAssembly.Instance(module, importObject);
    let t3 = Date.now();
    if (verbose)
        print(`new WebAssembly.Module(buf) took ${t3-t2} ms.`);

    return instance;
};

let stacktrace;
const importObject = { env: { _silly: i => { stacktrace = (new Error).stack; return i + 42; } } };
const instance = compile(wasmFile, importObject);
const result = instance.exports._parrot(1);
assert.eq(result, 1 + 42);

assert.truthy(stacktrace);
stacktrace = stacktrace.split("\n");
assert.falsy(stacktrace[0].indexOf("_silly") === -1);
assert.eq(stacktrace[1], "wasm-stub@[wasm code]"); // the wasm->js stub
assert.eq(stacktrace[2], "<?>.wasm-function[_eggs]@[wasm code]");
assert.eq(stacktrace[3], "<?>.wasm-function[_bacon]@[wasm code]");
assert.eq(stacktrace[4], "<?>.wasm-function[_spam]@[wasm code]");
assert.eq(stacktrace[5], "<?>.wasm-function[_parrot]@[wasm code]");
assert.eq(stacktrace[6], "wasm-stub@[wasm code]"); // wasm entry
