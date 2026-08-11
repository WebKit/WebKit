// Multi-VM WebAssembly Debugger Test - Different Functions
// This test creates multiple VMs running different functions from the SAME WebAssembly module
// to verify that the debugger properly handles different VMs executing different functions

// WASM module with 3 different functions
// Compiled from multi-funcs.wat using wat2wasm
const wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: (func [] -> [])
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

    // [0x0e] Function section: 3 functions (all type 0)
    0x03, 0x04, 0x03, 0x00, 0x00, 0x00,

    // [0x14] Export section: export 3 functions
    0x07, 0x19, 0x03,
    // Export function 0 as "func1"
    0x05, 0x66, 0x75, 0x6e, 0x63, 0x31, 0x00, 0x00,
    // Export function 1 as "func2"
    0x05, 0x66, 0x75, 0x6e, 0x63, 0x32, 0x00, 0x01,
    // Export function 2 as "func3"
    0x05, 0x66, 0x75, 0x6e, 0x63, 0x33, 0x00, 0x02,

    // [0x2f] Code section
    0x0a,                   // section id=10
    0x22,                   // section size=34
    0x03,                   // 3 functions

    // Function 0: func1 (body size=10)
    0x0a,                   // body size
    0x00,                   // 0 local declarations
    0x01,                   // [0x34] nop
    0x41, 0x0a,             // [0x35] i32.const 10
    0x1a,                   // [0x37] drop
    0x01,                   // [0x38] nop
    0x41, 0x14,             // [0x39] i32.const 20
    0x1a,                   // [0x3b] drop
    0x0b,                   // [0x3c] end

    // Function 1: func2 (body size=10)
    0x0a,                   // body size
    0x00,                   // 0 local declarations
    0x01,                   // [0x3f] nop
    0x41, 0x1e,             // [0x40] i32.const 30
    0x1a,                   // [0x42] drop
    0x01,                   // [0x43] nop
    0x41, 0x28,             // [0x44] i32.const 40
    0x1a,                   // [0x46] drop
    0x0b,                   // [0x47] end

    // Function 2: func3 (body size=10)
    0x0a,                   // body size
    0x00,                   // 0 local declarations
    0x01,                   // [0x4a] nop
    0x41, 0x32,             // [0x4b] i32.const 50
    0x1a,                   // [0x4d] drop
    0x01,                   // [0x4e] nop
    0x41, 0x3c,             // [0x4f] i32.const 60
    0x1a,                   // [0x51] drop
    0x0b                    // [0x52] end
]);

const NUM_WORKERS = 1;
const PRINT_INTERVAL = 1e7;

print("=== Multi-VM Different Functions Test ===");
print(`Creating ${NUM_WORKERS} workers + main thread (${NUM_WORKERS + 1} VMs total)`);
print("Each VM runs a DIFFERENT function from the same WASM module");
print("");

// Worker script: each worker runs a specific function
const workerScript = (workerId, funcName) => `
    const wasmBytes = new Uint8Array([${Array.from(wasm).join(',')}]);
    const wasmModule = new WebAssembly.Module(wasmBytes);
    const wasmInstance = new WebAssembly.Instance(wasmModule, {});

    // Run a specific wasm function in an infinite loop
    const func = wasmInstance.exports.${funcName};
    let iteration = 0;
    for (; ;) {
        func();
        iteration += 1;
        if (iteration % ${PRINT_INTERVAL} == 0)
            print("Worker ${workerId} (${funcName}) iteration=", iteration);
    }
`;

// Main thread runs func1
print("Main thread starting func1 execution...");
const mainModule = new WebAssembly.Module(wasm);
const mainInstance = new WebAssembly.Instance(mainModule, {});
const func1 = mainInstance.exports.func1;

// Worker runs func2 (different from main thread's func1)
const functions = ['func2'];
for (let i = 0; i < NUM_WORKERS; i++) {
    $.agent.start(workerScript(i + 1, functions[i]));
    print(`Started worker ${i + 1}/${NUM_WORKERS} running ${functions[i]}`);
}

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    func1();
    iteration += 1;
    if (iteration % PRINT_INTERVAL == 0)
        print("Main (func1) iteration=", iteration);
}
