// Like async-cache-hit-sync-return.js, but with many locals live across the
// cold-path await. Stresses the per-local cost paid by async function
// invocations that complete without suspending.

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
    const l0 = key.length;
    const l1 = l0 + 1;
    const l2 = l1 + 1;
    const l3 = l2 + 1;
    const l4 = l3 + 1;
    const l5 = l4 + 1;
    const l6 = l5 + 1;
    const l7 = l6 + 1;
    const fresh = await fetchSlow(key);
    cache.set(key, fresh);
    acc += fresh.value + l0 + l1 + l2 + l3 + l4 + l5 + l6 + l7;
    return fresh;
}

// Exercise the suspending miss path too.
let sink;
for (let j = 0; j < 8; j++)
    sink = getRecord('m' + j);
drainMicrotasks();

(function () {
    for (let i = 0; i < 2000000; i++)
        sink = getRecord(keys[i & 15]);
})();
drainMicrotasks();

// Miss path: key 'm0'..'m7' has length 2, so l0..l7 = 2..9, sum = 44.
const expected = 8 * (100 + 44) + (2000000 / 16) * 240;
if (acc !== expected)
    throw new Error("bad result: " + acc);
if (typeof sink !== "object")
    throw new Error("bad sink");
