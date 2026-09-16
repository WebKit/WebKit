function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected}, got ${actual}`);
}

// StringIteratorNext / StringIteratorNextWithUndefined via an explicit .next() call.
function stringIteratorNext(flag, n, q) {
    let last = "init";
    let acc = 0;
    let j = 0;
    do {
        let sv = (j & 1) ? "pq" : "rs";
        for (let i = 0; i < 4; i++) {
            if (i != 3 && flag)
                acc += q.x;
            else {
                let r = (i == 3 ? "abc" : sv)[Symbol.iterator]().next();
                last = r.value;
            }
        }
        j++;
    } while (j < n);
    return last;
}

// Same node, reached through the for-of fast iteration path instead.
function stringIteratorForOf(flag, n, q) {
    let last = "init";
    let acc = 0;
    let j = 0;
    do {
        let sv = (j & 1) ? "pq" : "rs";
        for (let i = 0; i < 4; i++) {
            if (i != 3 && flag)
                acc += q.x;
            else {
                for (const c of (i == 3 ? "abc" : sv)) {
                    last = c;
                    break;
                }
            }
        }
        j++;
    } while (j < n);
    return last;
}

// MapIteratorNext: like the above, this producer is safeToExecute() and declares no writes, so it
// is a hoisting candidate too.
function mapIteratorNext(flag, n, q, mv) {
    let last = 0;
    let acc = 0;
    let j = 0;
    const mc = new Map([[1, 11], [2, 22]]);
    do {
        for (let i = 0; i < 4; i++) {
            if (i != 3 && flag)
                acc += q.x;
            else {
                let r = (i == 3 ? mc : mv).values().next();
                last = r.value;
            }
        }
        j++;
    } while (j < n);
    return last;
}

// EnumeratorNextUpdateIndexAndMode.
function forIn(flag, n, q, ov) {
    let last = "";
    let acc = 0;
    let j = 0;
    const oc = { a: 1, b: 2 };
    do {
        let o2 = (j & 1) ? ov : q;
        for (let i = 0; i < 4; i++) {
            if (i != 3 && flag)
                acc += q.x;
            else {
                for (const k in (i == 3 ? oc : o2)) {
                    last = k;
                    break;
                }
            }
        }
        j++;
    } while (j < n);
    return last;
}

const q = { x: 1 };
const mv = new Map([[9, 99]]);
const ov = { z: 3 };

for (let k = 0; k < 1e5; k++) {
    stringIteratorNext((k & 1) == 0, 3, q);
    stringIteratorForOf((k & 1) == 0, 3, q);
    mapIteratorNext((k & 1) == 0, 3, q, mv);
    forIn((k & 1) == 0, 3, q, ov);
}

shouldBe(stringIteratorNext(true, 1, q), "a");
shouldBe(stringIteratorForOf(true, 1, q), "a");
shouldBe(mapIteratorNext(true, 1, q, mv), 11);
shouldBe(forIn(true, 1, q, ov), "a");
