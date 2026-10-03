// Two linear memories in one instance, deliberately different sizes and holding a different byte at
// the same offset, so a reader that ignored the memory index is caught by the size or the value.
//
// WASM Generation:
//     wat2wasm --enable-multi-memory --debug-names multi-memory.wat
//
//     (module
//       (memory $m0 1)          ;; 1 page  = 0x10000
//       (memory $m1 2)          ;; 2 pages = 0x20000
//       (data (memory $m0) (i32.const 16) "\aa")
//       (data (memory $m1) (i32.const 16) "\bb")
//       (func $load0 (export "load0") (result i32) i32.const 16 i32.load8_u)
//       (func $load1 (export "load1") (result i32) i32.const 16 i32.load8_u (memory $m1))
//     )
var wasm = new Uint8Array([
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: 1 function type
    0x01, 0x05,             // section id=1, size=5
    0x01,                   // 1 type
    0x60, 0x00, 0x01, 0x7f, // Type 0: (func [] -> [i32])

    // [0x0f] Function section: 2 functions, both type 0
    0x03, 0x03,             // section id=3, size=3
    0x02,                   // 2 functions
    0x00, 0x00,             // func[0], func[1] use type 0

    // [0x14] Memory section: 2 memories -- this is what needs multi-memory
    0x05, 0x05,             // section id=5, size=5
    0x02,                   // 2 memories
    0x00, 0x01,             // memory 0: no max, min 1 page  (0x10000 bytes)
    0x00, 0x02,             // memory 1: no max, min 2 pages (0x20000 bytes)

    // [0x1b] Export section: "load0", "load1"
    0x07, 0x11,             // section id=7, size=17
    0x02,                   // 2 exports
    0x05, 0x6c, 0x6f, 0x61, 0x64, 0x30, 0x00, 0x00, // "load0" -> func 0
    0x05, 0x6c, 0x6f, 0x61, 0x64, 0x31, 0x00, 0x01, // "load1" -> func 1

    // [0x2e] Code section
    0x0a, 0x12,             // section id=10, size=18
    0x02,                   // 2 function bodies
    0x07, 0x00,             // func[0] body size=7, 0 locals
    0x41, 0x10,             //   i32.const 16
    0x2d, 0x00, 0x00,       //   i32.load8_u align=0 offset=0        -- memory 0 implicitly
    0x0b,                   //   end
    0x08, 0x00,             // func[1] body size=8, 0 locals
    0x41, 0x10,             //   i32.const 16
    0x2d, 0x40, 0x01, 0x00, //   i32.load8_u align=0|0x40, memidx=1  -- an instruction immediate,
    0x0b,                   //   end                                    not part of the address

    // [0x42] Data section: one active segment per memory, same offset, different byte
    0x0b, 0x0e,             // section id=11, size=14
    0x02,                   // 2 segments
    0x00,                   //   segment 0: flags=0 -> memory 0
    0x41, 0x10, 0x0b,       //     offset (i32.const 16)
    0x01, 0xaa,             //     1 byte: 0xaa
    0x02, 0x01,             //   segment 1: flags=2 -> explicit memidx, memory 1
    0x41, 0x10, 0x0b,       //     offset (i32.const 16)
    0x01, 0xbb,             //     1 byte: 0xbb

    // [0x52] Custom "name" section: function names, so `b load0` resolves
    0x00, 0x28,
    0x04, 0x6e, 0x61, 0x6d, 0x65, // "name"
    0x01, 0x0f, 0x02,             // function names: 2 entries
    0x00, 0x05, 0x6c, 0x6f, 0x61, 0x64, 0x30, // func 0 = "load0"
    0x01, 0x05, 0x6c, 0x6f, 0x61, 0x64, 0x31, // func 1 = "load1"
    0x02, 0x05, 0x02,             // local names: 2 entries, none each
    0x00, 0x00, 0x01, 0x00,
    0x06, 0x09, 0x02,             // memory names: 2 entries
    0x00, 0x02, 0x6d, 0x30,       // memory 0 = "m0"
    0x01, 0x02, 0x6d, 0x31,       // memory 1 = "m1"
]);

var instance = new WebAssembly.Instance(new WebAssembly.Module(wasm));

// Assert here so a broken fixture fails loudly rather than making the debugger look wrong.
if (instance.exports.load0() !== 0xaa)
    print("FAIL: memory 0 [16] should be 0xaa, got " + instance.exports.load0());
if (instance.exports.load1() !== 0xbb)
    print("FAIL: memory 1 [16] should be 0xbb, got " + instance.exports.load1());

print("DEBUGGER_READY");

for (;;)
    instance.exports.load0();
