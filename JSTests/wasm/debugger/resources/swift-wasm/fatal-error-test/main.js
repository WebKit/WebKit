var wasm_code = read('fatal-error-test.wasm', 'binary');
var wasm_module = new WebAssembly.Module(wasm_code);

// fd_write must record nwritten so Swift's runtime doesn't retry the write forever.
var memory = null;

var imports = {
    wasi_snapshot_preview1: {
        proc_exit: function (code) {
            print("Program exited with code:", code);
        },
        args_get: function () { return 0; },
        args_sizes_get: function () { return 0; },
        environ_get: function () { return 0; },
        environ_sizes_get: function () { return 0; },
        fd_write: function (fd, iovs_ptr, iovs_len, nwritten_ptr) {
            if (memory) {
                var view = new DataView(memory.buffer);
                var total = 0;
                for (var i = 0; i < iovs_len; i++)
                    total += view.getUint32(iovs_ptr + i * 8 + 4, true);
                view.setUint32(nwritten_ptr, total, true);
            }
            return 0;
        },
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
memory = instance.exports.memory;

print("Available exports:", Object.keys(instance.exports));

let triggerFatalError = instance.exports.trigger_fatal_error;

print("DEBUGGER_READY");
let iteration = 0;
for (;;) {
    try {
        triggerFatalError(0);
    } catch (e) {
        // Catch the trap so the loop keeps running until lldb attaches.
    }
    iteration += 1;
    if (iteration % 1e3 == 0)
        print("iteration=", iteration);
}
