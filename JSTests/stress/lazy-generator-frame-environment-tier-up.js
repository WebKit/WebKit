function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// The generator frame environment is created lazily at the first suspend
// point. Run hot on the non-suspending path so the code tiers up, then take
// the suspending path for the first time inside optimized code.

const cache = new Map();
for (let j = 0; j < 16; j++)
    cache.set('k' + j, j);

async function fetchSlow(key) {
    return key.length * 1000;
}

async function getRecord(key) {
    const hit = cache.get(key);
    if (hit !== undefined)
        return hit;
    const fresh = await fetchSlow(key);
    cache.set(key, fresh);
    return fresh;
}

let sink;
for (let i = 0; i < testLoopCount; i++)
    sink = getRecord('k' + (i & 15));
drainMicrotasks();

let out1 = null, out2 = null;
getRecord('newkey').then(v => { out1 = v; });
drainMicrotasks();
shouldBe(out1, 6000);
getRecord('newkey').then(v => { out2 = v; });
drainMicrotasks();
shouldBe(out2, 6000);

function* maybeYield(n) {
    let extra = n * 2;
    if (n < 0) {
        yield extra;
        extra += 1;
        yield extra;
    }
    return extra;
}

for (let i = 0; i < testLoopCount; i++) {
    const it = maybeYield(i & 63);
    const r = it.next();
    shouldBe(r.done, true);
    shouldBe(r.value, (i & 63) * 2);
}
{
    const it = maybeYield(-5);
    shouldBe(it.next().value, -10);
    shouldBe(it.next().value, -9);
    const r = it.next();
    shouldBe(r.done, true);
    shouldBe(r.value, -9);
}
