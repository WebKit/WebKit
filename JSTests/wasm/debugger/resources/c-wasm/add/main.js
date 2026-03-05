// WASM Generation:
//     emcc -g -O0 -s STANDALONE_WASM=1 -s EXPORTED_FUNCTIONS='["_main","_add"]' add.c -o add.wasm
var wasm_code = read('add.wasm', 'binary');
var wasm_module = new WebAssembly.Module(wasm_code);
var imports = {
    wasi_snapshot_preview1: {
        proc_exit: function (code) {
            print("Program exited with code:", code);
        }
    }
};

var instance = new WebAssembly.Instance(wasm_module, imports);
let main = instance.exports.main;

let iteration = 0;
for (; ;) {
    main();
    iteration += 1;
    if (iteration % 1e5 == 0)
        print("iteration=", iteration);
}



