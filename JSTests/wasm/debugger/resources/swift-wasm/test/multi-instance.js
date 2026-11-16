// Two live instances of one Swift module, both running. Each instance is a separate library
// carrying the same DWARF, so a breakpoint set by symbol name resolves once per instance.
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

var first = new WebAssembly.Instance(wasm_module, imports);
var second = new WebAssembly.Instance(wasm_module, imports);

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    first.exports.process_number(iteration);
    second.exports.process_number(iteration);
    iteration += 1;
    if (iteration % 1e5 == 0)
        print("iteration=", iteration);
}
