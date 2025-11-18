// Multi-VM WebAssembly Debugger Test
// This test creates multiple VMs (main thread + 2 workers) running WebAssembly code concurrently
// to verify that the debugger properly handles cross-VM/instance scenarios

// Simple WASM module: (func [] -> [])
const wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: (func [] -> [])
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

    // [0x0e] Function section: 1 function
    0x03, 0x02, 0x01, 0x00,

    // [0x12] Export section: export function 0 as "compute"
    0x07, 0x0b, 0x01, 0x07, 0x63, 0x6f, 0x6d, 0x70, 0x75, 0x74, 0x65, 0x00, 0x00,

    // [0x1f] Code section
    0x0a,                   // section id=10
    0x09,                   // section size=9
    0x01,                   // 1 function
    0x07,                   // function body size=7
    0x00,                   // 0 local declarations
    0x01,                   // [0x24] nop
    0x41, 0x2a,             // [0x25] i32.const 42
    0x1a,                   // [0x27] drop
    0x01,                   // [0x28] nop
    0x0b                    // [0x29] end function
]);

const NUM_WORKERS = 2;
const PRINT_INTERVAL = 1e7;

print("=== Multi-VM WebAssembly Debugger Test ===");
print(`Creating ${NUM_WORKERS} workers + main thread (${NUM_WORKERS + 1} VMs total)`);
print("Each VM will run in an infinite loop - attach debugger to test");
print("");

// Worker script: each worker instantiates the wasm module and calls it in an infinite loop
const workerScript = (workerId) => `
    const wasmBytes = new Uint8Array([${Array.from(wasm).join(',')}]);
    const wasmModule = new WebAssembly.Module(wasmBytes);
    const wasmInstance = new WebAssembly.Instance(wasmModule, {});

    // Run the wasm function in an infinite loop
    const compute = wasmInstance.exports.compute;
    let iteration = 0;
    for (; ;) {
        compute();
        iteration += 1;
        if (iteration % ${PRINT_INTERVAL} == 0)
            print("Worker ${workerId} iteration=", iteration);
    }
`;

// Start workers
for (let i = 0; i < NUM_WORKERS; i++) {
    $.agent.start(workerScript(i + 1));
    print(`Started worker ${i + 1}/${NUM_WORKERS}`);
}

// Main thread also runs WebAssembly in an infinite loop
print("Main thread starting WebAssembly execution...");
print("");
const mainModule = new WebAssembly.Module(wasm);
const mainInstance = new WebAssembly.Instance(mainModule, {});
const mainCompute = mainInstance.exports.compute;

let iteration = 0;
for (; ;) {
    mainCompute();
    iteration += 1;
    if (iteration % PRINT_INTERVAL == 0)
        print("Main iteration=", iteration);
}
