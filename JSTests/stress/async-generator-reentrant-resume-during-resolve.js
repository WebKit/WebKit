// Regression test for https://bugs.webkit.org/show_bug.cgi?id=316132
//
// Resolving an async generator request reads `.then` on the iterator result object. A user-defined
// `then` getter on Object.prototype can reentrantly call `iter.next()` while the generator is
// suspended at a yield. The old driver resumed the generator nested inside that resolve and could
// leave it awaiting, then resumed it again from the outer drain — asserting/crashing.
//
// After aligning the driver to the current spec (AsyncGeneratorDrainQueue / AsyncGeneratorResume /
// AsyncGeneratorCompleteStep with a draining-queue state), the resolve in AsyncGeneratorYield runs
// while the generator is in an execution state, so the reentrant next() only enqueues; the request
// is then resumed inline. The observed order matches SpiderMonkey (the spec sequences CompleteStep,
// which fulfils the first next()'s promise, before the inline resume re-suspends on the awaited
// return value). It intentionally differs from V8's order; engines do not agree here.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

let log = [];
let iter;
let thenGets = 0;

Object.defineProperty(Object.prototype, "then", {
    get() {
        log.push("then:" + thenGets);
        if (thenGets++ === 0)
            iter.next();
        return undefined;
    },
    configurable: true,
});

async function* gen() {
    log.push("start");
    yield 1;
    log.push("resume");
    return 2;
}

let error = null;
let resolvedWith = null;

(async function main() {
    iter = gen();
    await iter.next();
    log.push("await-first");
    await 0;
    await 0;
    return 10;
})().then(result => { resolvedWith = result; }, e => { error = e; });

drainMicrotasks();

if (error)
    throw error;
shouldBe(log.join("|"), "start|then:0|resume|await-first|then:1");
shouldBe(resolvedWith, 10);
