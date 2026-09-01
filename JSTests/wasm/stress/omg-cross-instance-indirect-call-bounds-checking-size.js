//@ requireOptions("--useWasmFastMemory=true", "--useConcurrentJIT=false", "--thresholdForBBQOptimizeAfterWarmUp=1", "--thresholdForOMGOptimizeAfterWarmUp=10", "--thresholdForOMGOptimizeSoon=10")
//@ skip if $secAccel3Removed

import * as assert from "../assert.js";

// A cross-instance call_indirect out of an OMG function whose own memory is in Signaling mode must
// hand the callee its own bounds checking size in wasmBoundsCheckingSizeRegister. That register is
// not pinned in Signaling mode, so if the context switch that installs it is emitted as a separate
// B3 operation from the call, Air is free to allocate the register to an unrelated Tmp in between
// and the BoundsChecking callee ends up checking its accesses against that Tmp's value.
//
// The caller below is shaped to create the register pressure that makes the allocator do exactly
// that: 24 i64 locals kept live across a loop nest and then passed three times each as call
// arguments, so the poison value sits in a spilled Tmp whose fixup load lands between the context
// switch and the call.
//
// Both modules import their memory so the two modes this needs can be forced and asserted: a
// Signaling caller is what leaves the register unpinned, and a BoundsChecking callee is what
// actually reads it. Getting either mode wrong makes the test pass without exercising the bug.
//
// The modules are assembled by hand rather than through wabt-wrapper.js because the exact shape
// here was tuned against the register allocator; keeping it as explicit bytes keeps that shape
// from shifting under a wat round trip.

function leb(n) { let r = []; do { let b = n & 0x7f; n >>>= 7; if (n) b |= 0x80; r.push(b); } while (n); return r; }
function section(id, body) { return [id, ...leb(body.length), ...body]; }
function vec(items) { return [...leb(items.length), ...items.flat()]; }
function str(s) { return [...leb(s.length), ...[...s].map(c => c.charCodeAt(0))]; }

const N_I64 = 24;
const REPEAT = 3;
const LOOP_DEPTH = 6;
const PAGE_SIZE = 0x10000;

// memory import descriptor: "e"."mem", kind 0x02, limits {min: 1, max: 1}
const memoryImport = [...str("e"), ...str("mem"), 0x02, 0x01, 0x01, 0x01];

// callee: (i32 offset, i64 x N_I64 * REPEAT) -> ()
// caller: (i32 loopCond, i32 doCall, i32 offset, i64 x N_I64) -> ()
function buildCaller() {
    const sigParams = [0x7f, ...Array(N_I64 * REPEAT).fill(0x7e)];
    const fsigParams = [0x7f, 0x7f, 0x7f, ...Array(N_I64).fill(0x7e)];
    const types = section(1, vec([
        [0x60, ...vec(sigParams), ...vec([])],
        [0x60, ...vec(fsigParams), ...vec([])],
    ]));
    const imports = section(2, vec([memoryImport]));
    const funcs = section(3, vec([[0x01]]));
    const tables = section(4, vec([[0x70, 0x01, 0x01, 0x01]]));
    const exports = section(7, vec([
        [...str("f"), 0x00, 0x00],
        [...str("tbl"), 0x01, 0x00],
    ]));
    let body = [];
    for (let d = 0; d < LOOP_DEPTH; d++)
        body.push(0x03, 0x40);
    // Keep every i64 local live across the loop nest.
    for (let k = 0; k < N_I64; k++)
        body.push(0x41, 0x00, 0x20, ...leb(3 + k), 0x37, 0x03, ...leb(8 * k));
    for (let d = 0; d < LOOP_DEPTH; d++)
        body.push(0x20, 0x00, 0x0d, 0x00, 0x0b);
    body.push(0x20, 0x01);
    body.push(0x04, 0x40);
    body.push(0x20, 0x02);
    for (let r = 0; r < REPEAT; r++) {
        for (let k = 0; k < N_I64; k++)
            body.push(0x20, ...leb(3 + k));
    }
    body.push(0x41, 0x00);
    body.push(0x11, 0x00, 0x00); // call_indirect type=0 table=0
    body.push(0x0b);
    body.push(0x0b);
    const code = section(10, vec([[...leb(body.length + 1), 0x00, ...body]]));
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...types, ...imports, ...funcs, ...tables, ...exports, ...code,
    ]);
}

function buildCallee() {
    const sigParams = [0x7f, ...Array(N_I64 * REPEAT).fill(0x7e)];
    const types = section(1, vec([[0x60, ...vec(sigParams), ...vec([])]]));
    const imports = section(2, vec([memoryImport]));
    const funcs = section(3, vec([[0x00]]));
    const exports = section(7, vec([[...str("g"), 0x00, 0x00]]));
    // local.get 0; local.get 1; i64.store align=3 offset=0
    const body = [0x20, 0x00, 0x20, 0x01, 0x37, 0x03, 0x00, 0x0b];
    const code = section(10, vec([[...leb(body.length + 1), 0x00, ...body]]));
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...types, ...imports, ...funcs, ...exports, ...code,
    ]);
}

const callerMemory = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "Signaling");
const calleeMemory = createWebAssemblyMemoryWithMode({ initial: 1, maximum: 1 }, "BoundsChecking");
assert.eq(WebAssemblyMemoryMode(callerMemory), "Signaling");
assert.eq(WebAssemblyMemoryMode(calleeMemory), "BoundsChecking");

const callerInstance = new WebAssembly.Instance(new WebAssembly.Module(buildCaller()), { e: { mem: callerMemory } });
const f = callerInstance.exports.f;
const tbl = callerInstance.exports.tbl;

// The generator's m_mode comes from the instance, so assert it there too and not just on the memory.
assert.eq(WebAssemblyMemoryMode(callerInstance), "Signaling");

const calleeInstance = new WebAssembly.Instance(new WebAssembly.Module(buildCallee()), { e: { mem: calleeMemory } });
const g = calleeInstance.exports.g;
assert.eq(WebAssemblyMemoryMode(calleeInstance), "BoundsChecking");

const POISON = 0x4141414141414141n;
const poison = Array(N_I64).fill(POISON);
const zeros = Array(N_I64).fill(0n);
const calleeZeros = Array(N_I64 * REPEAT).fill(0n);
const calleePoison = Array(N_I64 * REPEAT).fill(POISON);

// The callee traps on this access when it is handed its own bounds checking size. Checking it up
// front catches a test whose own offset is no longer out of bounds, which would otherwise pass
// vacuously.
assert.throws(() => g(PAGE_SIZE, ...calleePoison), WebAssembly.RuntimeError, "Out of bounds memory access");

// Tier g past IPInt. IPInt's prologue reloads the bounds checking size register on entry, which
// would mask the bug; BBQ and OMG do not.
for (let i = 0; i < wasmTestLoopCount; i++)
    g(0, ...calleeZeros);
// Tier f to OMG without ever taking the call_indirect branch: doCall is 0 and the table is still
// empty, so the cross-instance call is first executed below, against OMG code.
for (let i = 0; i < wasmTestLoopCount; i++)
    f(0, 0, 0, ...zeros);

tbl.set(0, g);

// Repeat rather than call once: which register Air parks the poison in is an allocation decision,
// and f may be recompiled (e.g. under wasm-eager-jettison) between iterations.
for (let i = 0; i < wasmTestLoopCount; i++)
    assert.throws(() => f(0, 1, PAGE_SIZE, ...poison), WebAssembly.RuntimeError, "Out of bounds memory access");
