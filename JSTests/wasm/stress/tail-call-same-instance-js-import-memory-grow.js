//@ requireOptions("--useWasmFastMemory=false", "--wasmInliningMaximumWasmCalleeSize=0")
import * as assert from "../assert.js";
import { instantiate } from "../gc/wast-wrapper.js";

// A tail call whose target is a JS import always needs a restore frame to refresh
// the wasm memory base pointer cached in a callee save register.
//
// --useWasmFastMemory=false forces MemoryMode::BoundsChecking, where growing
// reallocates the buffer

const OLD_MARK = 0x13371337;   // present only in the pre-grow buffer
const NEW_MARK = 0x42424242;   // written only into the post-grow buffer
const STORE_VAL = 0x41414141;
const LOAD_OFF = 1024;
const STORE_OFF = 2048;

const CASES = [
    {
        name: "return_call",
        decls: ``,
        tailBody: `return_call $f`,
    },
    {
        name: "return_call_indirect",
        decls: `(table $t 1 1 funcref)
                (elem (i32.const 0) func $f)`,
        tailBody: `i32.const 0
                   return_call_indirect (type $sig)`,
    },
    {
        name: "return_call_ref",
        decls: `(elem declare func $f)`,
        tailBody: `ref.func $f
                   return_call_ref $sig`,
    },
    // Control: the non-tail form through the same funcref. addCallIndirect always
    // reloads the pinned registers on return, so this shape was never affected. It
    // is here to catch a "fix" that breaks or over-restores the non-tail path.
    {
        name: "call_indirect (control)",
        decls: `(table $t 1 1 funcref)
                (elem (i32.const 0) func $f)`,
        tailBody: `i32.const 0
                   call_indirect (type $sig)
                   return`,
    },
];

// $observe calls $tail (or $nop) and then accesses memory through the pinned
// registers. Selecting the arm by parameter lets $observe tier up while leaving
// $tail in whichever tier the caller asked for: warming with 1 tiers $tail up
// too, warming with 0 leaves it in IPInt.
function moduleFor(tailCase, tag) {
    return `(module
    (type $sig (func (result i32)))
    (import "env" "f" (func $f (type $sig)))
    (memory (export "mem") 16 32)
    ${tailCase.decls}

    ;; Keeps each case's module bytes distinct so that no two cases share a
    ;; compiled module, and with it each other's tier-up state.
    (func $unique (result i32) i32.const ${tag})

    (func $tail (type $sig)
        ${tailCase.tailBody})

    (func $nop (type $sig)
        i32.const 0)

    (func (export "observe") (param $useTail i32) (result i32)
        local.get $useTail
        if (result i32)
            call $tail
        else
            call $nop
        end
        drop
        i32.const ${STORE_OFF}
        i32.const ${STORE_VAL}
        i32.store
        i32.const ${LOAD_OFF}
        i32.load)
)`;
}

let nextTag = 1;

function test(tailCase, warmArg) {
    let grown = false;
    let mem = null;

    const f = () => {
        if (!grown)
            return 0;
        mem.grow(1);
        const u32 = new Uint32Array(mem.buffer);
        u32[LOAD_OFF >> 2] = NEW_MARK;
        // Warm-up left STORE_VAL here and grow's memcpy carried it over, so
        // clear it to tell a stale store from a correct one.
        u32[STORE_OFF >> 2] = 0;
        return 0;
    };

    const instance = instantiate(moduleFor(tailCase, nextTag++), { env: { f } });
    mem = instance.exports.mem;
    new Uint32Array(mem.buffer)[LOAD_OFF >> 2] = OLD_MARK;

    // Floor the warm-up so the JIT tiers are reached even in configurations where
    // wasmTestLoopCount collapses to a handful of iterations; the bug only shows up
    // once the caller keeps its memory base in a pinned register.
    const warmCount = Math.max(wasmTestLoopCount, 500);
    for (let i = 0; i < warmCount; i++)
        instance.exports.observe(warmArg);

    grown = true;
    const where = `${tailCase.name}, warm-up argument ${warmArg}`;
    assert.eq(instance.exports.observe(1) >>> 0, NEW_MARK,
        `load after ${where} read the pre-grow buffer`);
    assert.eq(new Uint32Array(mem.buffer)[STORE_OFF >> 2] >>> 0, STORE_VAL,
        `store after ${where} did not land in the current memory`);
}

for (const warmArg of [1, 0]) {
    for (const tailCase of CASES)
        test(tailCase, warmArg);
}
