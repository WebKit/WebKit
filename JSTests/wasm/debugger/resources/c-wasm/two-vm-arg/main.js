var wasm_code = read('two-vm-arg.wasm', 'binary');
var wasm_module = new WebAssembly.Module(wasm_code);
var imports = {
    wasi_snapshot_preview1: {
        proc_exit: function (code) {
            print("Program exited with code:", code);
        }
    }
};

var instance = new WebAssembly.Instance(wasm_module, imports);
const PRINT_INTERVAL = 1e6;

$.agent.start(`
    var bytes = new Uint8Array([${Array.from(wasm_code).join(',')}]);
    const wasmInstance = new WebAssembly.Instance(new WebAssembly.Module(bytes), {});

    let iteration = 0;
    for (; ;) {
        let result = wasmInstance.exports.compute(222, iteration);
        iteration += 1;
        if (iteration % ${PRINT_INTERVAL} == 0)
            print("Worker iteration=", iteration, " result=", result);
    }
`);

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    let result = instance.exports.compute(111, iteration);
    iteration += 1;
    if (iteration % PRINT_INTERVAL == 0)
        print("Main iteration=", iteration, " result=", result);
}
