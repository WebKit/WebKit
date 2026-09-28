//@ skip if $secAccel3Removed
//@ runDefault("--useFTLJIT=0", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeSoon=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--gcMaxHeapSize=64000000", "--verifyGC=1")

// StoreBarrierInsertionPhase stamps allocating nodes as fresh and elides the FencedStoreBarrier on
// stores into their results, which is sound only if no GC safepoint runs before the next doesGC()
// node. NewTypedArrayBuffer (emitted for `new ArrayBuffer(n)`) breaks that: its
// JSArrayBuffer::finishCreation calls Heap::addReference, which can start a collection once the cell
// already exists, so the collector can blacken the fresh buffer. A barrier-elided `buf[key] = victim`
// then writes an untracked black -> white edge, and victim is swept while still reachable through
// buf. --verifyGC=1 makes that a hard failure: after each collection the verifier re-marks from the
// roots and RELEASE_ASSERTs on any cell the real SlotVisitor did not mark.

const D = 100;       // chain depth: keeps victim off the machine stack behind a chain of loads
const W = 12;        // dummy props per link: ballast for the concurrent marker
const K = 150;       // delay rounds, so the marker pops and blackens buf before the store
const SZ = 1 << 20;  // external memory charged per iteration: paces the mid-node GC
const POOL = 256;    // chains prebuilt per epoch
const LAG = 512;     // how many buffers stay reachable
const ITER = 1000;   // race attempts: the bug is probabilistic, so this trades runtime for detection

function clobber(a, b, c, d, e, g, h, i) { return a ^ b ^ c ^ d ^ e ^ g ^ h ^ i; }
noInline(clobber);

let mkTail, mkLink;
{
    let props = [];
    for (let j = 0; j < W; ++j)
        props.push("b" + j + ": {}");
    mkTail = eval("(function (victim) { return { a: null, p: victim, " + props.join(", ") + " }; })");
    mkLink = eval("(function (next) { return { a: next, " + props.join(", ") + " }; })");
}

// victim: 1 inline property plus 3 out-of-line ones, so it owns an auxiliary butterfly too.
function makeChain(mark) {
    let victim = { m: mark };
    victim.o0 = mark + 0.125;
    victim.o1 = mark + 0.25;
    victim.o2 = mark + 0.375;
    let cur = mkTail(victim);
    for (let i = 0; i < D; ++i)
        cur = mkLink(cur);
    return cur;
}
noInline(makeChain);

// Overwrite the dead part of the machine stack so a swept victim is not conservatively re-rooted.
let scrub;
{
    let parts = [];
    for (let i = 0; i < 256; ++i)
        parts.push("a" + i + " = 0");
    scrub = eval("(function scrub() { let " + parts.join(", ") + "; return a0 + a255; })");
    noInline(scrub);
}

// Straight line, single basic block, no doesGC() node between the ArrayBuffer allocation and the
// store, so the store barrier really is elided.
let f;
{
    let src = "(obj, seed, key) => {\n";
    src += "    let buf = new ArrayBuffer(" + SZ + ");\n";
    src += "    let x = seed | 0;\n";
    for (let i = 0; i < K; ++i)
        src += "    x = (x ^ (x << 13)); x = (x ^ (x >>> 17)); x = (x ^ (x << 5));\n";
    let cur = "obj";
    for (let i = 0; i < D; ++i) {
        src += "    let v" + i + " = " + cur + ".a;\n";
        cur = "v" + i;
    }
    src += "    let victim = " + cur + ".p;\n";
    src += "    buf[key] = victim;\n";     // barrier-elided by-val IC transition store
    src += "    " + cur + ".p = null;\n";  // drop the only other path to victim
    src += "    victim = null;\n";
    for (let i = D - 1; i >= 0; --i)
        src += "    v" + i + " = null;\n";
    src += "    clobber(x, x, x, x, x, x, x, x);\n";
    src += "    return buf;\n}";
    f = eval(src);
}

let pool = new Array(POOL).fill(null);
function buildPool(baseMark) {
    for (let j = 0; j < POOL; ++j)
        pool[j] = makeChain(baseMark + j / 1024);
}
noInline(buildPool);

let live = new Array(LAG).fill(null);
let liveKey = new Array(LAG).fill(null);
let liveMark = new Array(LAG).fill(0);

// Secondary, JS-visible oracle: once the victim has been swept and its memory recycled, reading it
// back through the barrier-elided edge returns garbage.
function check(idx) {
    let buf = live[idx];
    if (buf === null)
        return;
    let victim = buf[liveKey[idx]];
    let mark = liveMark[idx];
    if (typeof victim !== "object" || victim === null)
        throw new Error("victim is not an object: " + victim);
    if (victim.m !== mark || victim.o0 !== mark + 0.125 || victim.o1 !== mark + 0.25 || victim.o2 !== mark + 0.375)
        throw new Error("victim was recycled: m=" + victim.m + " o0=" + victim.o0 + " o1=" + victim.o1 + " o2=" + victim.o2 + " expected " + mark);
}
noInline(check);

let seed = 0x2545F491;
for (let i = 0; i < ITER; ++i) {
    let e = i & (POOL - 1);
    if (e === 0) {
        buildPool(i + 0.5);
        scrub();
    }
    let key = (i & 1) ? "q" : "r";
    let head = pool[e];
    pool[e] = null;  // the chain is now reachable only through `head`
    scrub();
    let buf = f(head, seed + i, key);
    head = null;
    scrub();

    let idx = i % LAG;
    check(idx);  // the oldest still-reachable buffer: its collection cycle is long finished
    live[idx] = buf;
    liveKey[idx] = key;
    liveMark[idx] = (i - e) + 0.5 + e / 1024;
    buf = null;
    scrub();
}

for (let i = 0; i < LAG; ++i)
    check(i);
