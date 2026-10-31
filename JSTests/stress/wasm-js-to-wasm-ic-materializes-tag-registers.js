//@ runDefault("--useConcurrentJIT=false", "--useDFGJIT=false", "--thresholdForBBQOptimizeAfterWarmUp=10", "--thresholdForOMGOptimizeAfterWarmUp=100", "--thresholdForOMGOptimizeSoon=100")

// RTT::jsToWasmICEntrypoint() is entered without trustworthy JS tag registers, so every
// argument arm has to materialize them before any check that reads
// GPRInfo::notCellMaskRegister / GPRInfo::numberTagRegister. The I64 and funcref arms used to
// skip that and call branchIfNotCell() in its default HaveTagRegisters mode.
//
// notCellMaskRegister (x28 on ARM64) is not wasm-pinned, so OMG parks a live wasm i64 in it.
// The wasm->JS exit thunk leaves it alone, the JS callee's baseline prologue spills the stale
// value into its frame, and a strict-mode tail call restores it before far-jumping into the IC.
// With notCellMaskRegister == 0 the `value & notCellMask` test says "cell" for every JSValue, so
// branchIfNotHeapBigInt / emitLoadStructure dereference raw non-cell bits.
//
// Both arms must reject the non-cell arguments below by taking the IC slow path.

function leb(n) {
    const out = [];
    do {
        let b = n & 0x7f;
        n >>>= 7;
        if (n)
            b |= 0x80;
        out.push(b);
    } while (n);
    return out;
}

function str(s) {
    const bytes = [];
    for (let i = 0; i < s.length; i++)
        bytes.push(s.charCodeAt(i));
    return [...leb(bytes.length), ...bytes];
}

function section(id, content) {
    return [id, ...leb(content.length), ...content];
}

const I64 = 0x7e, EXTERNREF = 0x6f, FUNCREF = 0x70;

// (func (export "callee") (param T) (result T) local.get 0)
function buildCallee(type) {
    const typeSec = section(1, [1, 0x60, 1, type, 1, type]);
    const funcSec = section(3, [1, 0]);
    const exportSec = section(7, [1, ...str("callee"), 0x00, 0]);
    const body = [0, 0x20, 0, 0x0b];
    const codeSec = section(10, [1, ...leb(body.length), ...body]);
    return new Uint8Array([0x00, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
        ...typeSec, ...funcSec, ...exportSec, ...codeSec]);
}

// import "m" "imp" : (externref) -> T
// 10 mutable i64 globals, all 0
// (func (export "trigger") (param externref) (result T)
//   (local i64 x10)
//   local[1+i] = global[i]        ;; i in 0..9
//   call $imp(local 0)            ;; T result stays on the stack
//   global[i] = local[1+i]        ;; keeps all 10 i64 locals live across the call, so OMG is
// )                               ;; forced to allocate one of them into x27/x28
function buildCaller(resultType) {
    const NGLOBALS = 10;
    const typeSec = section(1, [1, 0x60, 1, EXTERNREF, 1, resultType]);
    const importSec = section(2, [1, ...str("m"), ...str("imp"), 0x00, 0]);
    const funcSec = section(3, [1, 0]);
    const globals = [];
    for (let i = 0; i < NGLOBALS; i++)
        globals.push(I64, 0x01, 0x42, 0x00, 0x0b);
    const globalSec = section(6, [NGLOBALS, ...globals]);
    const exportSec = section(7, [1, ...str("trigger"), 0x00, 1]);

    const body = [];
    body.push(1, NGLOBALS, I64);
    for (let i = 0; i < NGLOBALS; i++) {
        body.push(0x23, ...leb(i));      // global.get i
        body.push(0x21, ...leb(1 + i));  // local.set 1+i
    }
    body.push(0x20, 0);                  // local.get 0
    body.push(0x10, 0);                  // call $imp
    for (let i = 0; i < NGLOBALS; i++) {
        body.push(0x20, ...leb(1 + i));  // local.get 1+i
        body.push(0x24, ...leb(i));      // global.set i
    }
    body.push(0x0b);
    const codeSec = section(10, [1, ...leb(body.length), ...body]);
    return new Uint8Array([0x00, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
        ...typeSec, ...importSec, ...funcSec, ...globalSec, ...exportSec, ...codeSec]);
}

// `calleeType` is the wasm type of the argument arm under test; `warmUpValue` is a value that arm
// accepts on the fast path, and `cases` maps a non-cell JSValue to the outcome the IC slow path
// must produce for it (the same outcome the non-optimizing tiers produce).
function test(name, calleeType, warmUpValue, cases) {
    const callee = new WebAssembly.Instance(new WebAssembly.Module(buildCallee(calleeType))).exports.callee;

    function imp(x) {
        "use strict";
        return callee(x);  // strict mode -> op_tail_call -> restores the stale callee saves
    }

    const trigger = new WebAssembly.Instance(new WebAssembly.Module(buildCaller(calleeType)), { m: { imp } }).exports.trigger;

    // Link imp's call site monomorphically to callee's jsToWasmICEntrypoint...
    for (let i = 0; i < 2000; i++)
        imp(warmUpValue);
    // ...and tier trigger up to OMG so it allocates an i64 local into x27/x28.
    for (let i = 0; i < 1000; i++)
        trigger(warmUpValue);

    for (const [value, expected] of cases) {
        let actual;
        try {
            actual = String(trigger(value));
        } catch (e) {
            actual = `${e.constructor.name}: ${e.message}`;
        }
        if (actual !== expected)
            throw new Error(`${name}: trigger(${String(value)}) gave "${actual}", expected "${expected}"`);
    }
}

const invalidBigInt = "TypeError: Invalid argument type in ToBigInt operation";
const invalidFuncref = "TypeError: Argument value did not match the reference type";

// The I64 arm: branchIfNotCell then branchIfNotHeapBigInt, i.e. load8 [rawBits + 5].
// `true` is a non-cell that the slow path converts successfully (ToBigInt(true) is 1n) — it still
// crashes the unfixed fast path, which dereferences its raw bits.
test("i64", I64, 1n, [
    [undefined, invalidBigInt],
    [13.37, invalidBigInt],
    [null, invalidBigInt],
    [true, "1"],
]);

// The funcref arm: branchIfNotCell then emitLoadStructure, i.e. load32 [rawBits + 0].
// null takes the isNull fast path, so it is not a useful bad value here.
const someFuncref = new WebAssembly.Instance(new WebAssembly.Module(buildCallee(FUNCREF))).exports.callee;
test("funcref", FUNCREF, someFuncref, [
    [undefined, invalidFuncref],
    [13.37, invalidFuncref],
    [true, invalidFuncref],
]);
