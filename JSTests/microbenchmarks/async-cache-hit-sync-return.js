// Async function with an await only on the cache-miss path: the hot cache-hit
// path returns synchronously without ever suspending. Stresses the fixed cost
// of async function invocations that complete without reaching an await
// (memoized-async / cache-hit pattern).

const cache = new Map();
const keys = [];
for (let j = 0; j < 16; j++) {
    const key = 'k' + j;
    keys.push(key);
    cache.set(key, { id: j, value: j * 2 });
}

async function fetchSlow(key) {
    return { id: -1, value: 100, key };
}

let acc = 0;
async function getRecord(key) {
    const hit = cache.get(key);
    if (hit !== undefined) {
        acc += hit.value;
        return hit;
    }
    const fresh = await fetchSlow(key);
    cache.set(key, fresh);
    acc += fresh.value;
    return fresh;
}

// Exercise the suspending miss path too.
let sink;
for (let j = 0; j < 8; j++)
    sink = getRecord('m' + j);
drainMicrotasks();

(function () {
    for (let i = 0; i < 3000000; i++)
        sink = getRecord(keys[i & 15]);
})();
drainMicrotasks();

const expected = 8 * 100 + (3000000 / 16) * 240;
if (acc !== expected)
    throw new Error("bad result: " + acc);
if (typeof sink !== "object")
    throw new Error("bad sink");
