// A memory64 linear memory larger than 4 GiB, so offsets a 32-bit address field cannot hold actually
// exist: 0xaa at offset 0, 0xbb at 0x100000010. The 4 GiB is address space, not resident memory.
//
// WASM Generation:
//     wat2wasm --enable-memory64 memory64.wat
//
//     (module
//       (memory (export "mem") i64 65537)     ;; 65537 pages = 4 GiB + 64 KiB
//       (data (i64.const 0) "\aa")
//       (data (i64.const 0x100000010) "\bb")
//       (func (export "low")  (result i32) i64.const 0x0         i32.load8_u)
//       (func (export "high") (result i32) i64.const 0x100000010 i32.load8_u)
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

    // [0x14] Memory section: one i64 memory -- this is what needs memory64
    0x05, 0x05,             // section id=5, size=5
    0x01,                   // 1 memory
    0x04,                   // limits flags: bit 2 = i64 index type, no maximum
    0x81, 0x80, 0x04,       // min = 65537 pages (LEB128) = 0x100010000 bytes

    // [0x1b] Export section: "mem", "low", "high"
    0x07, 0x14,             // section id=7, size=20
    0x03,                   // 3 exports
    0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00,             // "mem"  -> memory 0
    0x03, 0x6c, 0x6f, 0x77, 0x00, 0x00,             // "low"  -> func 0
    0x04, 0x68, 0x69, 0x67, 0x68, 0x00, 0x01,       // "high" -> func 1

    // [0x31] Code section
    0x0a, 0x15,             // section id=10, size=21
    0x02,                   // 2 function bodies
    0x07, 0x00,             // func[0] body size=7, 0 locals
    0x42, 0x00,             //   i64.const 0                 -- i64 address operand
    0x2d, 0x00, 0x00,       //   i32.load8_u align=0 offset=0
    0x0b,                   //   end
    0x0b, 0x00,             // func[1] body size=11, 0 locals
    0x42, 0x90, 0x80, 0x80, 0x80, 0x10, // i64.const 4294967312 (0x100000010)
    0x2d, 0x00, 0x00,       //   i32.load8_u align=0 offset=0
    0x0b,                   //   end

    // [0x48] Data section: one byte low, one byte past 4 GiB
    0x0b, 0x11,             // section id=11, size=17
    0x02,                   // 2 segments
    0x00,                   //   segment 0: memory 0
    0x42, 0x00, 0x0b,       //     offset (i64.const 0)
    0x01, 0xaa,             //     1 byte: 0xaa
    0x00,                   //   segment 1: memory 0
    0x42, 0x90, 0x80, 0x80, 0x80, 0x10, 0x0b, //  offset (i64.const 0x100000010)
    0x01, 0xbb,             //     1 byte: 0xbb
]);

var instance = new WebAssembly.Instance(new WebAssembly.Module(wasm));

// Assert here so a broken fixture fails loudly rather than making the debugger look wrong.
if (instance.exports.mem.buffer.byteLength !== 0x100010000)
    print("FAIL: memory should be 0x100010000 bytes, got 0x" + instance.exports.mem.buffer.byteLength.toString(16));
if (instance.exports.low() !== 0xaa)
    print("FAIL: offset 0x0 should be 0xaa, got " + instance.exports.low());
if (instance.exports.high() !== 0xbb)
    print("FAIL: offset 0x100000010 should be 0xbb, got " + instance.exports.high());

print("DEBUGGER_READY");

for (;;)
    instance.exports.low();
