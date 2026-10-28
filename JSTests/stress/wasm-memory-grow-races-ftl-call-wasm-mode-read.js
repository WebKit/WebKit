//@ skip if $secAccel3Removed
//@ slow!
//@ runDefault("--useWasmFastMemory=0", "--useConcurrentJIT=true")

// Regression test for rdar://186122719.
//
// This is a probabilistic test that pays off over many ASan runs. Raise ROUNDS to make one run
// more likely to catch it.

const ROUNDS = 25;
const CALLERS = 12;        // distinct FTL plans per round, all on one instance
const CALL_SITES = 16;     // CallWasm nodes per plan
const WARMUP = 200000;     // executions needed to drive one caller to FTL tier-up
const MAX_PAGES = 129;     // grows per round is MAX_PAGES - 1

// (module
//   (import "env" "mem" (memory 0 4096))
//   (func (export "store") (param i32) (i32.store (i32.const 0) (local.get 0))))
const WASM_BYTES = new Uint8Array([
    0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 1, 127, 0, 2, 14, 1, 3, 101, 110, 118,
    3, 109, 101, 109, 2, 1, 0, 128, 32, 3, 2, 1, 0, 7, 9, 1, 5, 115, 116, 111, 114,
    101, 0, 0, 10, 11, 1, 9, 0, 65, 0, 32, 0, 54, 2, 0, 11
]);
const wasmModule = new WebAssembly.Module(WASM_BYTES);

let body = "";
for (let i = 0; i < CALL_SITES; ++i)
    body += `store(v + ${i});`;

// Each caller needs its own FunctionExecutable, or it shares an already-compiled
// CodeBlock and no new FTL plan is queued. `store` is a write-once const in the
// enclosing scope so the DFG folds it to a WebAssemblyFunction constant, which is
// what lets strength reduction turn the calls into CallWasm nodes.
function makeCaller(uid) {
    return eval(`(function () {
        const store = globalThis.INSTANCE.exports.store;
        return function caller${uid}(v) { ${body} };
    })()`);
}

let uid = 0;
for (let round = 0; round < ROUNDS; ++round) {
    const mem = new WebAssembly.Memory({ initial: 1, maximum: MAX_PAGES });
    globalThis.INSTANCE = new WebAssembly.Instance(wasmModule, { env: { mem } });

    const growEvery = Math.floor((CALLERS * WARMUP) / (MAX_PAGES - 1));
    let pages = 1;
    let tick = 0;

    for (let k = 0; k < CALLERS; ++k) {
        const caller = makeCaller(uid++);
        for (let i = 0; i < WARMUP; ++i) {
            caller(i | 0);
            if (++tick >= growEvery && pages < MAX_PAGES) {
                tick = 0;
                mem.grow(1);
                ++pages;
            }
        }
    }
}
