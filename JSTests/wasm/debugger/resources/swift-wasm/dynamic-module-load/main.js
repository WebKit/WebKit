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

var moduleA = new WebAssembly.Module(read('func-a.wasm', 'binary'));
var instance1 = new WebAssembly.Instance(moduleA, imports);
print("Module A (func_a) loaded");

print("DEBUGGER_READY");
print("Waiting for debugger — attach LLDB and type 'c' to continue.");
while (!$vm.hasDebuggerContinued()) { }
print("Debugger continued");

instance1.exports.func_a(1);

var moduleB = new WebAssembly.Module(read('func-b.wasm', 'binary'));
var instance2 = new WebAssembly.Instance(moduleB, imports);
print("Module B (func_b) loaded");

instance2.exports.func_b(1);
print("Done.");
