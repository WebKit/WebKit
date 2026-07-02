var wasm_code = read('crash-test.wasm', 'binary');
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

let crashOnZero = instance.exports.crash_on_zero;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    try {
        crashOnZero(0);
    } catch (e) {
        // When the debugger is not yet connected the Swift fatalError propagates as a
        // JS exception. Catch and retry so the process stays alive until LLDB connects.
        //
        // When the debugger IS connected, hitFault() blocks the VM before the exception
        // reaches here, so this catch is never taken on a debugger-intercepted fault.
    }
    iteration += 1;
    if (iteration % 1e5 == 0)
        print("iteration=", iteration);
}
