//@ skip if !$isWasmPlatform
//@ runDefault("--useDollarVM=1", "--useWasmMultiMemory=1", "--useWasmFastMemory=0", "--useConcurrentJIT=0")

// Build minimal Wasm module with 2 memories and a function that loads from memory 1
function buildModule() {
    const bytes = [];
    function emit(...bs) { bs.forEach(b => bytes.push(b & 0xFF)); }
    function emitLEB(v) {
        do {
            let byte = v & 0x7F;
            v >>>= 7;
            if (v) byte |= 0x80;
            bytes.push(byte);
        } while (v);
    }
    function section(id, content) {
        emit(id);
        emitLEB(content.length);
        content.forEach(b => bytes.push(b & 0xFF));
    }

    // Wasm header
    emit(0x00, 0x61, 0x73, 0x6D); // magic
    emit(0x01, 0x00, 0x00, 0x00); // version 1

    // Type section: two types
    // Type 0: (i32) -> (i32)  - for load function
    // Type 1: (i32, i32) -> () - for store function
    section(0x01, [
        0x02,                    // 2 types
        0x60, 0x01, 0x7F, 0x01, 0x7F,  // (i32) -> (i32)
        0x60, 0x02, 0x7F, 0x7F, 0x00,  // (i32, i32) -> ()
    ]);

    // Function section: 2 functions
    section(0x03, [
        0x02,       // 2 functions
        0x00,       // func 0 uses type 0
        0x01,       // func 1 uses type 1
    ]);

    // Memory section: 2 memories, each 1 page
    section(0x05, [
        0x02,       // 2 memories
        0x00, 0x01, // memory 0: no max, initial 1 page
        0x00, 0x01, // memory 1: no max, initial 1 page
    ]);

    // Export section: export both functions and memory 1
    section(0x07, [
        0x03,       // 3 exports
        // "load" -> func 0
        0x04, 0x6C, 0x6F, 0x61, 0x64, 0x00, 0x00,
        // "store" -> func 1
        0x05, 0x73, 0x74, 0x6F, 0x72, 0x65, 0x00, 0x01,
        // "mem1" -> memory 1
        0x04, 0x6D, 0x65, 0x6D, 0x31, 0x02, 0x01,
    ]);

    // Code section
    // func 0: (i32 addr) -> i32  { return i32.load8_u mem1[addr] }
    const func0Body = [
        0x00,             // 0 locals
        0x20, 0x00,       // local.get 0 (addr)
        0x2D,             // i32.load8_u
        0x40,             // align=0 with multi-memory flag (bit 6)
        0x01,             // memory index = 1
        0x00,             // offset = 0
        0x0B,             // end
    ];
    // func 1: (i32 addr, i32 val) -> void  { i32.store8 mem1[addr] = val }
    const func1Body = [
        0x00,             // 0 locals
        0x20, 0x00,       // local.get 0 (addr)
        0x20, 0x01,       // local.get 1 (val)
        0x3A,             // i32.store8
        0x40,             // align=0 with multi-memory flag (bit 6)
        0x01,             // memory index = 1
        0x00,             // offset = 0
        0x0B,             // end
    ];

    const codeBodies = [];
    codeBodies.push(func0Body.length);
    func0Body.forEach(b => codeBodies.push(b));
    codeBodies.push(func1Body.length);
    func1Body.forEach(b => codeBodies.push(b));

    const codeContent = [0x02]; // 2 function bodies
    codeBodies.forEach(b => codeContent.push(b));

    section(0x0A, codeContent);

    return new Uint8Array(bytes);
}

const PAGE_SIZE = 65536;

(function() {
    const wasmBytes = buildModule();
    const module = new WebAssembly.Module(wasmBytes);
    const instance = new WebAssembly.Instance(module);

    // Fill memory 1 with a known pattern near the boundary
    const mem1 = new Uint8Array(instance.exports.mem1.buffer);
    for (let i = PAGE_SIZE - 16; i < PAGE_SIZE; i++)
        mem1[i] = 0xAA;

    // Warm up both functions so OMG compiles them
    for (let i = 0; i < 200000; i++) {
        instance.exports.load(i & 0xFF);
        instance.exports.store(i & 0xFF, 0x42);
    }

    // Sanity: load last valid byte (offset PAGE_SIZE - 1)
    mem1[PAGE_SIZE - 1] = 0xBB;
    let val = instance.exports.load(PAGE_SIZE - 1);
    if (val != 0xBB) throw new Error("sanity check failed");

    // BUG: load at exactly PAGE_SIZE (first invalid byte)
    // For i32.load8_u, sizeOfOp=1, lastLoadedOffset = 0 + 1 - 1 = 0
    // OMG check: pointer + 0 > memorySize  =>  65536 > 65536  =>  false  => NO TRAP
    // Correct:   pointer + 0 >= memorySize  =>  65536 >= 65536 =>  true   => TRAP
    let oob = false;
    try {
        let oobVal = instance.exports.load(PAGE_SIZE);
        oob = true;
    } catch (e) {
    }
    if (oob) throw new Error("out of bounds 1");

    // BUG: store at exactly PAGE_SIZE (1-byte OOB write)
    oob = false;
    try {
        instance.exports.store(PAGE_SIZE, 0xDE);
        oob = true;
    } catch (e) {
    }
    if (oob) throw new Error("out of bounds 2");
})();
