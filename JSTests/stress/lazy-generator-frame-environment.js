function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// The generator frame environment is created lazily at the first suspend
// point. Cover cases where the first suspend differs per invocation, where
// the frame is reused across many suspends, and where no suspend happens.

// First suspend at a different await depending on the branch taken.
async function branchy(mode, v) {
    let a = v * 10;
    if (mode === 0) {
        const r = await Promise.resolve(1);
        return a + r;
    }
    const r2 = await Promise.resolve(2);
    return a * r2 + a;
}

// Synchronous return: the await is never reached, no frame is created.
const cache = new Map();
cache.set('hit', 42);
async function getRecord(key) {
    const hit = cache.get(key);
    if (hit !== undefined)
        return hit;
    const fresh = await Promise.resolve(key.length);
    cache.set(key, fresh);
    return fresh;
}

// Frame created at the first yield and reused by later suspends.
function* pick(mode) {
    let base = 100;
    if (mode === 0) {
        yield base + 1;
        base += 1;
    } else {
        yield base + 2;
        base += 2;
    }
    yield base;
}

// Sent values live across multiple resumes.
function* acc3() {
    const a = yield 1;
    const b = yield 2;
    const c = yield 3;
    return a + b + c;
}

for (let i = 0; i < testLoopCount; i++) {
    let out0 = null, out1 = null, hit = null, miss = null;
    branchy(0, 3).then(v => { out0 = v; });
    branchy(1, 3).then(v => { out1 = v; });
    getRecord('hit').then(v => { hit = v; });
    getRecord('miss' + (i & 7)).then(v => { miss = v; });
    drainMicrotasks();
    shouldBe(out0, 31);
    shouldBe(out1, 90);
    shouldBe(hit, 42);
    shouldBe(miss, 5);

    const a = pick(0);
    shouldBe(a.next().value, 101);
    shouldBe(a.next().value, 101);
    shouldBe(a.next().done, true);
    const b = pick(1);
    shouldBe(b.next().value, 102);
    shouldBe(b.next().value, 102);
    shouldBe(b.next().done, true);

    const it = acc3();
    it.next();
    it.next(10);
    it.next(20);
    const r = it.next(12);
    shouldBe(r.done, true);
    shouldBe(r.value, 42);

    // return()/throw() before the first suspend: no frame ever created.
    const g = pick(0);
    shouldBe(g.return(7).value, 7);
    const g2 = pick(0);
    let caught = false;
    try {
        g2.throw(new Error('boom'));
    } catch (e) {
        caught = e.message === 'boom';
    }
    shouldBe(caught, true);
}
