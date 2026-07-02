// Exercises both the converted (PerformPromiseThenOneHandler) and non-converted
// (PerformPromiseThen) paths produced by DFGConstantFoldingPhase. Each test
// function is noInline'd and run in a hot loop so DFG/FTL tier-up is forced.
//
// Conversion happens in DFGConstantFoldingPhase only when the abstract interpreter
// has *proven* that one handler is SpecFunction and the other is SpecOther
// (undefined or null). Otherwise we keep the generic 4-child PerformPromiseThen
// node, whose runtime semantics this test also verifies.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' vs ' + expected);
}

async function shouldReject(p, expected) {
    try {
        await p;
    } catch (e) {
        shouldBe(e, expected);
        return;
    }
    throw new Error('expected rejection with ' + expected);
}

// --- Converted FulfillHandler kind: .then(fn) — onRejected is implicit undefined ---
function fulfillOnly(p) {
    return p.then(v => v + 1);
}
noInline(fulfillOnly);

// --- Converted RejectHandler kind: .then(undefined, fn) ---
function rejectOnlyUndefined(p) {
    return p.then(undefined, e => 'caught:' + e);
}
noInline(rejectOnlyUndefined);

// --- Converted RejectHandler kind: .then(null, fn) — null is also SpecOther ---
function rejectOnlyNull(p) {
    return p.then(null, e => 'null-caught:' + e);
}
noInline(rejectOnlyNull);

// --- Non-converted: two callable handlers — neither side is SpecOther ---
function bothHandlers(p) {
    return p.then(v => 'ok:' + v, e => 'err:' + e);
}
noInline(bothHandlers);

// --- Non-converted: a non-callable, non-Other literal as fulfill handler ---
// AI proves SpecInt32, which is neither SpecFunction nor SpecOther → keep PerformPromiseThen.
// Runtime treats non-callable as no-handler → fulfill value passes through; the
// rejection handler runs on rejection.
function intHandlerWithReject(p) {
    return p.then(1, e => 'int-rej:' + e);
}
noInline(intHandlerWithReject);

async function main() {
    for (let i = 0; i < testLoopCount; ++i) {
        // 1. Converted fulfill, pending → fast inline write.
        {
            let resolve, p = new Promise(r => { resolve = r; });
            let q = fulfillOnly(p);
            resolve(i);
            shouldBe(await q, i + 1);
        }

        // 2. Converted fulfill, already-fulfilled → JIT precondition fails on status,
        // takes inline slow branch (calls operationPerformPromiseThenOneHandler).
        shouldBe(await fulfillOnly(Promise.resolve(i)), i + 1);

        // 3. Converted fulfill, rejection passes through (no onRejected → propagation).
        await shouldReject(fulfillOnly(Promise.reject('boom-' + i)), 'boom-' + i);

        // 4. Converted reject (undefined fulfill), pending → fast inline write.
        {
            let reject, p = new Promise((_, rj) => { reject = rj; });
            let q = rejectOnlyUndefined(p);
            reject('e-' + i);
            shouldBe(await q, 'caught:e-' + i);
        }

        // 5. Converted reject (undefined fulfill), already-rejected → slow branch.
        shouldBe(await rejectOnlyUndefined(Promise.reject('rej-' + i)), 'caught:rej-' + i);

        // 6. Converted reject, fulfillment passes through.
        shouldBe(await rejectOnlyUndefined(Promise.resolve('keep-' + i)), 'keep-' + i);

        // 7. Converted reject (null fulfill), pending.
        {
            let reject, p = new Promise((_, rj) => { reject = rj; });
            let q = rejectOnlyNull(p);
            reject('n-' + i);
            shouldBe(await q, 'null-caught:n-' + i);
        }

        // 8. Non-converted (two handlers), fulfilled side.
        shouldBe(await bothHandlers(Promise.resolve(i)), 'ok:' + i);

        // 9. Non-converted (two handlers), rejected side.
        shouldBe(await bothHandlers(Promise.reject('x-' + i)), 'err:x-' + i);

        // 10. Non-converted (two handlers), pending → fulfilled.
        {
            let resolve, p = new Promise(r => { resolve = r; });
            let q = bothHandlers(p);
            resolve(i);
            shouldBe(await q, 'ok:' + i);
        }

        // 11. Non-converted (two handlers), pending → rejected.
        {
            let reject, p = new Promise((_, rj) => { reject = rj; });
            let q = bothHandlers(p);
            reject('p-' + i);
            shouldBe(await q, 'err:p-' + i);
        }

        // 12. Non-converted (int handler), fulfillment passes through.
        shouldBe(await intHandlerWithReject(Promise.resolve('pass-' + i)), 'pass-' + i);

        // 13. Non-converted (int handler), rejection runs user handler.
        shouldBe(await intHandlerWithReject(Promise.reject('bang-' + i)), 'int-rej:bang-' + i);

        // 14. Multi-await on the same pending promise: first .then takes the
        // inline-fast path; the second/third force a spill into a real
        // JSSlimPromiseReaction chain.
        {
            let resolve, p = new Promise(r => { resolve = r; });
            let a = fulfillOnly(p);
            let b = fulfillOnly(p);
            let c = fulfillOnly(p);
            resolve(i);
            shouldBe(await a, i + 1);
            shouldBe(await b, i + 1);
            shouldBe(await c, i + 1);
        }

        // 15. isHandledFlag pre-set on a pending promise, with no reaction yet
        // (state=Pending, kind=None, payload=null). The JIT fast-path precondition
        // mask intentionally ignores isHandledFlag, so the fast path must still
        // proceed correctly: the OR of isHandledFlag into m_packed is a no-op and
        // the reaction is installed as normal. This state is unreachable from
        // pure JS, so we use $vm.markPromiseAsHandled to construct it.
        {
            let resolve, p = new Promise(r => { resolve = r; });
            $vm.markPromiseAsHandled(p);
            let q = fulfillOnly(p);
            resolve(i);
            shouldBe(await q, i + 1);
        }

        // 16. Same as above for the RejectHandler kind.
        {
            let reject, p = new Promise((_, rj) => { reject = rj; });
            $vm.markPromiseAsHandled(p);
            let q = rejectOnlyUndefined(p);
            reject('h-' + i);
            shouldBe(await q, 'caught:h-' + i);
        }
    }
}

main().then(
    () => {},
    e => { print('FAIL: ' + (e && e.stack || e)); $vm.abort(); }
);
