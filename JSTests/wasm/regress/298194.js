// Module with structs A/B (B is a non-final supertype, A holds a mutable ref to B)
// and exported functions new_A(ref B) -> ref A and new_B() -> ref B.
// Bytes captured from the WasmModuleBuilder GC DSL.
const mainBytes = new Uint8Array([
    // ── magic + version ──
    0x00, 0x61, 0x73, 0x6d,        // "\0asm"
    0x01, 0x00, 0x00, 0x00,        // version 1

    // ── Type section (id 1, size 0x18=24) ──
    0x01, 0x18,
    0x04,                          // 4 types
    // type 0 = B: sub (0 supertypes) of a struct with 0 fields (empty supertype struct)
    0x50, 0x00,                    // sub, 0 explicit supertypes
    0x5f, 0x00,                    //   struct, 0 fields
    // type 1 = A: sub (0 supertypes) of a struct with 1 mutable field (ref null 0 / ref null B)
    0x50, 0x00,                    // sub, 0 explicit supertypes
    0x5f, 0x01,                    //   struct, 1 field
    0x64, 0x00, 0x01,              //     field: (ref null 0) mutable
    // type 2 = new_A signature: (ref null 0 / B) -> (ref null 1 / A)
    0x60,                          // func
    0x01, 0x64, 0x00,              //   1 param: (ref null 0)
    0x01, 0x64, 0x01,              //   1 result: (ref null 1)
    // type 3 = new_B signature: () -> (ref null 0 / B)
    0x60,                          // func
    0x00,                          //   0 params
    0x01, 0x64, 0x00,              //   1 result: (ref null 0)

    // ── Function section (id 3, size 3) ──
    0x03, 0x03,
    0x02,                          // 2 functions
    0x02,                          // func 0 (new_A) : type 2
    0x03,                          // func 1 (new_B) : type 3

    // ── Export section (id 7, size 0x11=17) ──
    0x07, 0x11,
    0x02,                          // 2 exports
    0x05, 0x6e, 0x65, 0x77, 0x5f, 0x41, 0x00, 0x00,  // "new_A" -> func 0
    0x05, 0x6e, 0x65, 0x77, 0x5f, 0x42, 0x00, 0x01,  // "new_B" -> func 1

    // ── Code section (id 10, size 0x0f=15) ──
    0x0a, 0x0f,
    0x02,                          // 2 bodies
    // body 0 (new_A), size 7:
    0x07,
    0x00,                          //   0 locals
    0x20, 0x00,                    //   local.get 0
    0xfb, 0x00, 0x01,              //   struct.new 1 (create A)
    0x0b,                          //   end
    // body 1 (new_B), size 5:
    0x05,
    0x00,                          //   0 locals
    0xfb, 0x01, 0x00,              //   struct.new 0 (create B)
    0x0b,                          //   end

    // ── Custom "name" section (id 0, size 0x16=22) ──
    0x00, 0x16,
    0x04, 0x6e, 0x61, 0x6d, 0x65,  // name section: "name"
    0x01, 0x0f,                    // subsection 1 (function names), size 15
    0x02,                          //   2 name entries
    0x00, 0x05, 0x6e, 0x65, 0x77, 0x5f, 0x41,  //   func 0 -> "new_A"
    0x01, 0x05, 0x6e, 0x65, 0x77, 0x5f, 0x42,  //   func 1 -> "new_B"
]);

// Empty module: bytes captured from (new WasmModuleBuilder()).toBuffer().
const dummyBytes = new Uint8Array([
    // ── magic + version (only) ──
    0x00, 0x61, 0x73, 0x6d,        // "\0asm"
    0x01, 0x00, 0x00, 0x00,        // version 1
]);

function createWasmModule() {
    return new WebAssembly.Instance(new WebAssembly.Module(mainBytes));
}

let obj = null;
{
    let inst = createWasmModule();
    let exports = inst.exports;
    let objB = exports.new_B();
    obj = exports.new_A(objB);
}

gc();

let dummy = new WebAssembly.Instance(new WebAssembly.Module(dummyBytes));
dummy = null;

// Trigger TypeInformation::tryCleanup() to free the TypeDefinition.
gc();

// Trigger assertion failure during GC.
gc();
