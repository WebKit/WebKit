// The async-from-sync fast path builds/reuses the primordial sync iterator directly, skipping the observable
// Call(@@iterator) that the generic path performs. That elision is only sound while the iterator protocol is
// primordial (watchpoint/structure guarded). This test confirms the guard: an iterable that overrides
// @@iterator must fall back to the generic path and actually run the custom @@iterator, while a pristine
// iterable still produces the right values via the fast path.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

// A pristine Array/Map/Set/String takes the fast path; values must be correct.
async function pristine() {
    let s = [];
    for await (const x of [1, 2, 3]) s.push(x);
    assert(s.join(",") === "1,2,3", `array: ${s.join(",")}`);

    s = [];
    for await (const [k, v] of new Map([["a", 1], ["b", 2]])) s.push(k + v);
    assert(s.join(",") === "a1,b2", `map: ${s.join(",")}`);

    s = [];
    for await (const x of new Set([7, 8, 9])) s.push(x);
    assert(s.join(",") === "7,8,9", `set: ${s.join(",")}`);

    s = [];
    for await (const c of "xyz") s.push(c);
    assert(s.join(",") === "x,y,z", `string: ${s.join(",")}`);

    // Iterator-as-iterable (arr.keys()): @@iterator returns itself; must be reused, not re-created.
    s = [];
    for await (const k of [11, 22, 33].keys()) s.push(k);
    assert(s.join(",") === "0,1,2", `array keys: ${s.join(",")}`);
}

// An Array carrying its OWN @@iterator is not primordial: the fast path must NOT trigger, and the custom
// @@iterator must be invoked exactly once, with its values observed.
async function customOwnIterator() {
    let calls = 0;
    const arr = [10, 20, 30]; // element values are decoys; the custom iterator yields different values.
    arr[Symbol.iterator] = function () {
        calls++;
        let i = 0;
        return {
            next() {
                if (i < 3)
                    return { value: (i++ + 1) * 100, done: false };
                return { value: undefined, done: true };
            }
        };
    };
    const s = [];
    for await (const x of arr)
        s.push(x);
    assert(calls === 1, `custom @@iterator called ${calls} times, expected 1`);
    assert(s.join(",") === "100,200,300", `custom iterator sequence: ${s.join(",")}`);
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount;
    for (let i = 0; i < N; i++) {
        await pristine();
        await customOwnIterator();
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
