//@ skip if !$isWasmPlatform
//@ runDefault("--useWasmMultiMemory=1", "--useWasmFastMemory=0")

// Tests multi-memory OOB bounds checking in IPInt across three code paths:
//   - i32.load (scalar)        -> loadStoreMakePointerSlow .memoryIsNotZero
//   - v128.load (SIMD bulk)    -> metadataMemoryMakePointer .memoryIsNotZero
//   - v128.load64_lane (SIMD lane) -> ipintCheckMemoryBoundAndMakePointer

function leb128(v) {
    const r = [];
    do { let b = v & 0x7F; v >>>= 7; if (v) b |= 0x80; r.push(b); } while (v);
    return r;
}

function str(s) { return [...s].map(c => c.charCodeAt(0)); }

function buildModule() {
    const b = [];
    const push = (...x) => b.push(...x);
    const pushSec = (id, content) => {
        push(id); push(...leb128(content.length)); push(...content);
    };

    // Header
    push(0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00);

    // Type section: 2 types
    pushSec(1, [
        2,
        0x60, 1, 0x7F, 1, 0x7F,  // type 0: (i32) -> (i32)
        0x60, 1, 0x7F, 0,        // type 1: (i32) -> ()
    ]);

    // Function section: 3 functions
    pushSec(3, [3, 0, 1, 1]);

    // Memory section: mem0 (10 pages), mem1 (1 page)
    pushSec(5, [2, 0x00, 10, 0x00, 1]);

    // Export section
    const n0 = str("scalarLoad"), n1 = str("simdLoad"), n2 = str("simdLane64Load");
    pushSec(7, [
        3,
        n0.length, ...n0, 0x00, 0x00,
        n1.length, ...n1, 0x00, 0x01,
        n2.length, ...n2, 0x00, 0x02,
    ]);

    // Code section

    // func 0: i32.load from memory 1 (4-byte access)
    //   -> loadStoreMakePointerSlow
    const f0 = [
        0x00,                    // 0 locals
        0x20, 0x00,              // local.get 0
        0x28,                    // i32.load
        0x42, 0x01, 0x00,        // memarg: align=2|0x40, memidx=1, offset=0
        0x0B,                    // end
    ];

    // func 1: v128.load from memory 1, drop (16-byte access)
    //   -> metadataMemoryMakePointer (via simdMemoryOp)
    const f1 = [
        0x00,                    // 0 locals
        0x20, 0x00,              // local.get 0
        0xFD, 0x00,              // v128.load
        0x44, 0x01, 0x00,        // memarg: align=4|0x40, memidx=1, offset=0
        0x1A,                    // drop
        0x0B,                    // end
    ];

    // func 2: v128.load64_lane from memory 1, drop (8-byte access)
    //   -> ipintCheckMemoryBoundAndMakePointer
    //   Using 64-bit lane (not 8-bit) so size-1 != 0 and actually tests the
    //   size accounting in the bounds check.
    const f2 = [
        0x00,                                                    // 0 locals
        0x20, 0x00,                                              // local.get 0
        0xFD, 0x0C, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          // v128.const 0
        0xFD, 0x57,                                              // v128.load64_lane
        0x43, 0x01, 0x00,                                        // memarg: align=3|0x40, memidx=1, offset=0
        0x00,                                                    // lane 0
        0x1A,                                                    // drop
        0x0B,                                                    // end
    ];

    pushSec(10, [
        3,
        ...leb128(f0.length), ...f0,
        ...leb128(f1.length), ...f1,
        ...leb128(f2.length), ...f2,
    ]);

    return new Uint8Array(b);
}

const inst = new WebAssembly.Instance(new WebAssembly.Module(buildModule()));
const PAGE = 65536;

// i32.load: 4 bytes -> last valid addr = PAGE-4, first invalid = PAGE-3
inst.exports.scalarLoad(PAGE - 4);
try { inst.exports.scalarLoad(PAGE - 3); $vm.abort(); } catch (e) {}

// v128.load: 16 bytes -> last valid addr = PAGE-16, first invalid = PAGE-15
inst.exports.simdLoad(PAGE - 16);
try { inst.exports.simdLoad(PAGE - 15); $vm.abort(); } catch (e) {}

// v128.load64_lane: 8 bytes -> last valid addr = PAGE-8, first invalid = PAGE-7
inst.exports.simdLane64Load(PAGE - 8);
try { inst.exports.simdLane64Load(PAGE - 7); $vm.abort(); } catch (e) {}
