var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic: \0asm
    0x01, 0x00, 0x00, 0x00, // version: 1

    // [0x08] Type section: 1 type, func () -> ()
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

    // [0x0e] Function section: 1 function, type index 0
    0x03, 0x02, 0x01, 0x00,

    // [0x12] Export section: export function 0 as "crasher"
    0x07, 0x0b, 0x01, 0x07, 0x63, 0x72, 0x61, 0x73, 0x68, 0x65, 0x72, 0x00, 0x00,

    // [0x1f] Code section
    0x0a,       // section id = 10
    0x06,       // section size = 6
    0x01,       // 1 function body
    0x04,       // body size = 4
    0x00,       // 0 local declarations
    // The leading nop is load-bearing. A prologue interrupt is reported at the first body
    // instruction, so with `unreachable` first the PC is already on the site at resume, and
    // LLDB's lift dance (z0, s, Z0) can never complete: the step traps instead of retiring an
    // instruction, so the PC never moves and Z0 arrives after the stop.
    // FIXME: Drop the nop once stepping off a trapping site works -- same limitation as
    // MultiInstanceUnreachableOwnSiteTestCase.
    0x01,       // [0x24] nop
    0x00,       // [0x25] unreachable
    0x0b        // [0x26] end
]);

var module = new WebAssembly.Module(wasm);
var instance = new WebAssembly.Instance(module);
let crasher = instance.exports.crasher;

print("DEBUGGER_READY");
let iteration = 0;
for (; ;) {
    try {
        crasher();
    } catch (e) {

    }
    iteration += 1;
    if (iteration % 1e5 == 0)
        print("iteration=", iteration);
}
