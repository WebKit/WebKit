// Coverage for op_async_iterator_open's FAST path (genuine async generator -> driver sentinel) across
// tier-ups, and its getNext-checkpoint deopt/fallthrough. A single for-await site is warmed fastEligible
// with primordial async generators, then fed shapes that stress the fast path's getNext GetById + the
// CompareEqPtr reclassify:
//   - polymorphic instance structures (many distinct async generator functions),
//   - perturbed yielded value types (Int32 -> double -> object), to force speculation OSR exits whose
//     exit origins sit around the symbolCall/getNext checkpoints,
//   - instances whose .next is non-primordial (own data property / accessor): the fast path fetches .next,
//     CompareEqPtr fails, and it falls through (keepBlock) to the generic real-call next path,
//   - instances whose @@asyncIterator is overridden: the DFG eligibility CompareEqPtr fails, taking the
//     generic symbolCall path.
// After warmup, %AsyncGeneratorPrototype%.next is wrapped globally so the fast site's runtime CompareEqPtr
// flips true->false, exercising the deopt-to-real-next at an already-FTL site. Every consume() must return
// the exact sum the generic path would, proving the fast-path fetch/reclassify/deopt is transparent.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

const AGP = Object.getPrototypeOf(Object.getPrototypeOf((async function* () {})()));
const primordialNext = AGP.next;

// The single warmed for-await site every case funnels through.
async function consume(iterable) {
    let sum = 0;
    for await (const x of iterable)
        sum += x;
    return sum;
}

// Distinct generator functions -> distinct instance structures, to make the open site's getNext base polymorphic.
const genFns = [];
for (let i = 0; i < 6; i++)
    genFns.push(async function* () { yield 1; yield 2; yield 3; });

let error = null;
let done = false;

async function main() {
    // Phase 1: monomorphic primordial -> fastEligible + sentinel, Int32 values.
    for (let k = 0; k < testLoopCount; k++)
        assert(await consume((async function* () { yield 1; yield 2; yield 3; })()) === 6, "phase1");

    // Phase 2: polymorphic instance structures at the same site.
    for (let k = 0; k < testLoopCount; k++)
        assert(await consume(genFns[k % genFns.length]()) === 6, "phase2");

    // Phase 3: perturb yielded value types to force speculation OSR exits around the open/next checkpoints.
    for (let k = 0; k < testLoopCount; k++) {
        const kk = k & 0xff;
        async function* g() { yield 0.5; yield kk + 0.25; yield { valueOf() { return 2; } }; }
        assert(await consume(g()) === 0.5 + (kk + 0.25) + 2, "phase3");
    }

    // Phase 4: non-primordial .next as an own DATA property. Fast path fetches it, CompareEqPtr false ->
    // keepBlock -> generic real-call next. .next is read once by getNext, then called per step (2 values + done).
    for (let k = 0; k < testLoopCount; k++) {
        const g = (async function* () { yield 10; yield 20; })();
        let calls = 0;
        Object.defineProperty(g, "next", { configurable: true, writable: true,
            value: function (...a) { calls++; return primordialNext.apply(this, a); } });
        assert(await consume(g) === 30, "phase4 sum");
        assert(calls === 3, "phase4 calls=" + calls);
    }

    // Phase 5: non-primordial .next as an ACCESSOR. The fast path's getNext GetById invokes the getter
    // exactly once (fetch), CompareEqPtr false, fall through to generic real-call next.
    for (let k = 0; k < testLoopCount; k++) {
        const g = (async function* () { yield 5; yield 7; })();
        let reads = 0;
        Object.defineProperty(g, "next", { configurable: true, get() { reads++; return primordialNext; } });
        assert(await consume(g) === 12, "phase5 sum");
        assert(reads === 1, "phase5 reads=" + reads);
    }

    // Phase 6: overridden @@asyncIterator on the instance. The DFG eligibility CompareEqPtr on m_symbolIterator
    // fails -> generic symbolCall (calls the custom @@asyncIterator, which returns the generator itself).
    for (let k = 0; k < testLoopCount; k++) {
        const g = (async function* () { yield 100; yield 200; })();
        let iterCalls = 0;
        g[Symbol.asyncIterator] = function () { iterCalls++; return this; };
        assert(await consume(g) === 300, "phase6 sum");
        assert(iterCalls === 1, "phase6 iterCalls=" + iterCalls);
    }

    // Phase 7: globally wrap %AsyncGeneratorPrototype%.next AFTER the site is hot. The fast path's runtime
    // CompareEqPtr(fetched, primordial) now fails for every genuine async generator, deopting the warmed FTL
    // site to the generic real-call next path. Results must be unchanged.
    let wrapCalls = 0;
    AGP.next = function (...a) { wrapCalls++; return primordialNext.apply(this, a); };
    try {
        for (let k = 0; k < testLoopCount; k++)
            assert(await consume((async function* () { yield 1; yield 2; yield 3; })()) === 6, "phase7");
        assert(wrapCalls > 0, "phase7 wrapper should have been invoked");
    } finally {
        AGP.next = primordialNext;
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
