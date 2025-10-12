var wasm_code = read('test.wasm', 'binary');
var wasm_module = new WebAssembly.Module(wasm_code);
var imports = {
    wasi_snapshot_preview1: {
        proc_exit: function (code) {
            print("Program exited with code:", code);
        },
        args_get: function () { return 0; },
        args_sizes_get: function () { return 0; },
        environ_get: function () { return 0; },
        environ_sizes_get: function () { return 0; },
        fd_write: function () { return 0; },
        fd_read: function () { return 0; },
        fd_close: function () { return 0; },
        fd_seek: function () { return 0; },
        fd_fdstat_get: function () { return 0; },
        fd_prestat_get: function () { return 8; },
        fd_prestat_dir_name: function () { return 8; },
        path_open: function () { return 8; },
        random_get: function () { return 0; },
        clock_time_get: function () { return 0; }
    }
};

var instance = new WebAssembly.Instance(wasm_module, imports);

print("Available exports:", Object.keys(instance.exports));

let processNumber = instance.exports.process_number;

let iteration = 0;
for (; ;) {
    processNumber(iteration);
    iteration += 1;
    if (iteration % 1e5 == 0)
        print("iteration=", iteration);
}


