// This test is statistical, and without optimizing Wasm tiers the likelihood
// of hitting the deepest expected stack trace is very low, so disable.
// FIXME: remove when 32-bit platforms support optimizing Wasm tiers
//@ skip if ["arm", "mips"].include?($architecture)
//
//@ runDefault

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

if (platformSupportsSamplingProfiler() && $vm.isWasmSupported()) {
    const verbose = false;
    const wasmFile = './sampling-profiler/nameSection.wasm';

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

    const importObject = { env: { _silly: i => {
        var result = 0;
        for (var i = 0; i < 2000000; ++i)
            result++;
        return result;
    } } };
    const instance = compile(wasmFile, importObject);
    const result = instance.exports._parrot(1);

    load("./sampling-profiler/samplingProfiler.js", "caller relative");
    var wasmEntry = function() {
        return instance.exports._parrot(1);
    };
    runTest(wasmEntry, ["_silly", "(unknown)", "<?>.wasm-function[_eggs]", "<?>.wasm-function[_bacon]", "<?>.wasm-function[_spam]", "<?>.wasm-function[_parrot]", "wasm-stub", "wasmEntry"]);
}
