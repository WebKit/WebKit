// WASM debugger test for memory.atomic.wait32 with no timeout (infinite wait).
//
// A worker thread calls waiter() with timeout=-1, blocking indefinitely.
// The main thread is idle (JSC forbids memory.atomic.wait on the main thread).
// This exercises the STW deadlock fix: the debugger must be able to interrupt
// a VM that is blocked inside memory.atomic.wait with an infinite timeout.
//
// Module layout (single function "waiter"):
//   [0x2a] i32.const 0         ; address
//   [0x2c] i32.const 0         ; expected value
//   [0x2e] i64.const -1        ; timeout (-1 = wait indefinitely)
//   [0x30] memory.atomic.wait32 align=2 offset=0
//   [0x34] end
//
// Virtual addresses (worker compiles first → base 0x4000000000000000):
//   memory.atomic.wait32 → 0x4000000000000030
//   end                  → 0x4000000000000034

var wasmBytes = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: (func [] -> [i32])
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,

    // [0x0f] Function section: 1 function of type 0
    0x03, 0x02, 0x01, 0x00,

    // [0x13] Memory section: 1 shared memory min=1 max=1
    // flags=0x03: min+max+shared
    0x05, 0x04, 0x01, 0x03, 0x01, 0x01,

    // [0x19] Export section: "waiter" -> func 0
    0x07, 0x0a, 0x01,
    0x06, 0x77, 0x61, 0x69, 0x74, 0x65, 0x72, 0x00, 0x00, // "waiter", func 0

    // [0x25] Code section
    0x0a, 0x0e, 0x01,

    // [0x28] Function 0 body: memory.atomic.wait32(addr=0, expected=0, timeout=-1)
    0x0c, 0x00,             // body size=12, 0 locals
    0x41, 0x00,             // [0x2a] i32.const 0   (address)
    0x41, 0x00,             // [0x2c] i32.const 0   (expected)
    0x42, 0x7f,             // [0x2e] i64.const -1  (timeout: wait indefinitely)
    0xfe, 0x01, 0x02, 0x00, // [0x30] memory.atomic.wait32 align=2 offset=0
    0x0b,                   // [0x34] end
]);

// Worker compiles and runs waiter() with an infinite timeout.
// Bytes are inlined in the script string — the worker is the first to call
// WebAssembly.Module(), so it gets base address 0x4000000000000000.
$.agent.start(`
    var module = new WebAssembly.Module(new Uint8Array([${Array.from(wasmBytes).join(',')}]));
    var instance = new WebAssembly.Instance(module, {});
    var waiter = instance.exports.waiter;
    // FIXME: Even this is printed. The worker may still not be blocked before debugger attaches.
    print("DEBUGGER_READY");
    // Each call blocks indefinitely until notified or the debugger interrupts.
    for (;;)
        waiter();
`);

let iteration = 0;
for (;;) {
    iteration += 1;
    if (iteration % 1e8 == 0)
        print("Main iteration=", iteration);
}
