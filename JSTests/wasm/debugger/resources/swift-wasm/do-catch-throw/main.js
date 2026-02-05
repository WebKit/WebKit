var wasm_code = read('do-catch-throw.wasm', 'binary');
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

let testDoThrowCatch = instance.exports.test_do_throw_catch;
let testDoThrowCatchNested = instance.exports.test_do_throw_catch_nested;
let testDoThrowFuncCatch = instance.exports.test_do_throw_func_catch;
let testDoThrowFuncCatchNested = instance.exports.test_do_throw_func_catch_nested;

// Test 1: Direct throw in do block
print("\n=== Test 1: do-throw-catch (direct throw) ===");
print("test_do_throw_catch(2):", testDoThrowCatch(2));   // Even: throws, returns 42
print("test_do_throw_catch(3):", testDoThrowCatch(3));   // Odd: no throw, returns 23

// Test 2: Nested do-catch with direct throw
print("\n=== Test 2: do-throw-catch-nested (direct throw, nested) ===");
print("test_do_throw_catch_nested(2):", testDoThrowCatchNested(2));   // Even: inner throw → outer catch, returns 42
print("test_do_throw_catch_nested(3):", testDoThrowCatchNested(3));   // Odd: no throw, returns 23

// Test 3: Throw via function call
print("\n=== Test 3: do-throw-func-catch (function call) ===");
print("test_do_throw_func_catch():", testDoThrowFuncCatch());  // Expected: 42

// Test 4: Nested do-catch with function call
print("\n=== Test 4: do-throw-func-catch-nested (function call, nested) ===");
print("test_do_throw_func_catch_nested():", testDoThrowFuncCatchNested());  // Expected: 42

print("\n=== Running continuous loop ===");
let iteration = 0;
for (; ;) {
    testDoThrowCatch(2);
    testDoThrowCatchNested(2);
    testDoThrowFuncCatch();
    testDoThrowFuncCatchNested();
    iteration += 1;
    if (iteration % 1e5 == 0)
        print("iteration=", iteration);
}
