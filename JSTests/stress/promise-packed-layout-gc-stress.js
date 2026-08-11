// Exercise JSPromise under GC pressure to validate the new packed layout's
// visitChildren / write-barrier discipline. Allocates many promises, awaits
// them at various stages, and forces edenGC / fullGC between phases so the
// concurrent collector has a chance to scan promises that hold inline-handler
// payloads (a JSPromise* in m_packed) and inline-microtask contexts (in m_slot).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected}, got ${actual}`);
}

function defer() {
    let resolve, reject;
    let p = new Promise((res, rej) => { resolve = res; reject = rej; });
    return { promise: p, resolve, reject };
}

async function stressOne(i) {
    // Inline-microtask reaction (await on a freshly-pending promise). Force a
    // GC after attaching the await — the pending promise has its inline-microtask
    // context cell in m_slot and must be traced.
    let a = defer();
    let aPromise = a.promise.then(v => v); // re-await via .then to attach a reaction first
    edenGC();
    a.resolve(i);
    shouldBe(await aPromise, i);

    // Inline-child reaction (.then on pending). Force GC while the inline
    // FulfillHandler holds the result-promise pointer in m_packed and the
    // handler in m_slot.
    let b = defer();
    let c = b.promise.then(v => v + 1);
    edenGC();
    b.resolve(i * 2);
    shouldBe(await c, i * 2 + 1);

    // Inline-child reaction followed by spill (second consumer creates a
    // JSSlimPromiseReaction). GC at both states.
    let d = defer();
    let e1 = d.promise.then(v => v + 1);
    edenGC();
    let e2 = d.promise.then(v => v + 2);
    edenGC();
    d.resolve(i * 3);
    shouldBe(await e1, i * 3 + 1);
    shouldBe(await e2, i * 3 + 2);

    // Rejection with settlement value that's a fresh object (cell settlement
    // value lives in m_slot after settlement).
    let f = Promise.reject({ err: "e" + i });
    edenGC();
    try { await f; } catch (err) { shouldBe(err.err, "e" + i); }

    // Chain of 4 where each step uses inline-child. GC mid-chain.
    let g = Promise.resolve(i).then(x => x + 1).then(x => x * 2).then(x => { edenGC(); return x - i; });
    shouldBe(await g, (i + 1) * 2 - i);

    // Periodic full GC to exercise the slow path and unreachable-promise sweeping.
    if ((i % 32) === 0)
        fullGC();
}

async function main() {
    const N = 200;
    for (let i = 0; i < N; i++)
        await stressOne(i);
}

main().then(
    () => {},
    $vm.abort
);
