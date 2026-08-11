//@ runDefault("--useConcurrentJIT=true", "--numberOfWasmCompilerThreads=1")

// Tuning knobs — increase any of these to widen the race window if the test
// stops reproducing the bug on a new machine / after unrelated JSC changes:

const NFUNCS = 40000;
const NINSTRS = 20;
const NCOMPS = 16;
const KEEPALIVE_MS = 500;
const REFILL_COUNT = 128;
const REFILL_GLOBALS = 20;
const REFILL_DELAY_MS = 200;
const SCRUB_DEPTH = 30;

function leb(n) {
    const out = [];
    do {
        let b = n & 0x7f;
        n >>>= 7;
        if (n) b |= 0x80;
        out.push(b);
    } while (n);
    return out;
}
function section(id, payload) { return [id, ...leb(payload.length), ...payload]; }

function buildModule(nfuncs, ninstrs) {
    const body = [];
    for (let i = 0; i < ninstrs; i++) body.push(0x41, 0x01);
    for (let i = 1; i < ninstrs; i++) body.push(0x6a);
    body.push(0x0b);
    const code = [0x00, ...body];
    const codeEntry = [...leb(code.length), ...code];

    const typeSec = section(1, [0x01, 0x60, 0x00, 0x01, 0x7f]);
    const funcSec = section(3, [...leb(nfuncs), ...new Array(nfuncs).fill(0)]);
    const codePayload = [...leb(nfuncs)];
    for (let i = 0; i < nfuncs; i++)
        for (let b of codeEntry) codePayload.push(b);
    const codeSec = section(10, codePayload);

    return new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
                           ...typeSec, ...funcSec, ...codeSec]);
}

const bytes = buildModule(NFUNCS, NINSTRS);

// Keepalive bounds total runtime; pre-fix crashes within ~400ms; post-fix
// exits cleanly once the wasm worklist finishes (~1s on a fast machine).
setTimeout(function keepalive() {}, KEEPALIVE_MS);

// Kick off compiles in a secondary global, then drop it. Each compile captures
// a ticket + payloads into a heap SharedTask that fires on the Wasm worklist.
(function setup() {
    let g = createGlobalObject();
    for (let i = 0; i < NCOMPS; i++)
        g.WebAssembly.compile(bytes);
    g = null;
})();

// Clear conservative-stack refs, then full GC. End-of-GC cancels the tickets
// for the dropped global and arms a 0s timer; doWork then removes and frees
// the cancelled Ticket entries, opening the slot-reuse window.
function scrub(d) {
    if (d > 0) return scrub(d - 1) + 1;
    for (let i = 0; i < 200; i++) new Array(4).fill({});
    gc();
    gc();
    return 0;
}
scrub(SCRUB_DEPTH);

// Allocate fresh tickets so freed TZone slots get reused; pre-fix the
// worklist's stale pointer matches a reused slot → wrong-target callback.
// Delays are short so these timers can fire before the keepalive and don't
// extend the test runtime.
function nop() {}
function refill() {
    for (let i = 0; i < REFILL_COUNT; i++)
        setTimeout(nop, REFILL_DELAY_MS);
    for (let i = 0; i < REFILL_GLOBALS; i++)
        createGlobalObject().eval("1");
}
setTimeout(refill, 50);
setTimeout(refill, 150);
