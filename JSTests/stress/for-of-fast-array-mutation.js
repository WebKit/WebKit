// A for-of over an Array that runs without an iterator object must visit exactly what an Array Iterator driven by hand
// visits, whatever happens to the Array while the loop runs: the length is read again at every step, holes read through
// the prototype chain, getters run, and the storage may change kind under the loop.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function guarded(mutate) { return (a, step, seen) => { try { mutate(a, step, seen); } catch (e) { seen.push("E:" + e.constructor.name); } }; }

function show(x) { return x === undefined ? "u" : typeof x === "object" && x !== null ? "o" : String(x); }

function viaForOf(array, mutate) {
    let seen = [];
    let step = 0;
    for (let x of array) {
        seen.push(show(x));
        mutate(array, step++, seen);
        if (step > 30)
            break;
    }
    return seen.join();
}

function viaIterator(array, mutate) {
    let seen = [];
    let step = 0;
    let iterator = array[Symbol.iterator]();
    for (let r = iterator.next(); !r.done; r = iterator.next()) {
        seen.push(show(r.value));
        mutate(array, step++, seen);
        if (step > 30)
            break;
    }
    return seen.join();
}

function viaDestructuring(array, mutate) {
    let seen = [];
    let [a, b = (mutate(array, 0, seen), "d1"), c = (mutate(array, 1, seen), "d2"), d, ...rest] = array;
    return [a, b, c, d].map(show).join() + "|" + rest.map(show).join();
}

function viaDestructuringByHand(array, mutate) {
    let seen = [];
    let iterator = array[Symbol.iterator]();
    let values = [];
    let done = false;
    function step() { if (done) return undefined; let r = iterator.next(); if (r.done) { done = true; return undefined; } return r.value; }
    let a = step();
    let b = step(); if (b === undefined) { mutate(array, 0, seen); b = "d1"; }
    let c = step(); if (c === undefined) { mutate(array, 1, seen); c = "d2"; }
    let d = step();
    let rest = [];
    while (!done) { let r = iterator.next(); if (r.done) { done = true; break; } rest.push(r.value); }
    return [a, b, c, d].map(show).join() + "|" + rest.map(show).join();
}

let makers = [
    () => [1, 2, 3, 4, 5],
    () => [1.5, 2.5, 3.5],
    () => ["a", {}, null, undefined, "e"],
    () => [1, , 3, , 5, , ],
    () => { let a = [1, 2, 3]; a.length = 6; return a; },
    () => { let a = [1, 2, 3]; a[30] = 31; return a; },
    () => [],
    () => { let a = [1, 2, 3, 4]; Object.defineProperty(a, 1, { get() { return "getter"; }, configurable: true }); return a; },
    () => { let a = []; for (let i = 0; i < 12; i++) a.push(i); return a; },
];

let mutations = [
    (a, step) => { },
    (a, step) => { if (step === 1) a.push("pushed"); },
    (a, step) => { if (step < 5) a.push("p" + step); },
    (a, step) => { if (step === 1) a.pop(); },
    (a, step) => { if (step === 1) a.length = 2; },
    (a, step) => { if (step === 0) a.length = 0; },
    (a, step) => { if (step === 1) { a.length = 0; a.push("x", "y", "z", "w"); } },
    (a, step) => { if (step === 1) a.shift(); },
    (a, step) => { if (step === 1) a.unshift("u0", "u1"); },
    (a, step) => { if (step === 1) a.splice(1, 2); },
    (a, step) => { if (step === 0) a[2] = 0.5; },
    (a, step) => { if (step === 0) a[2] = "string"; },
    (a, step) => { if (step === 0) delete a[2]; },
    (a, step) => { if (step === 0) a[a.length + 20] = "far"; },
    (a, step) => { if (step === 1) a.reverse(); },
    (a, step) => { if (step === 0) Object.defineProperty(a, 3, { get() { return "late"; }, configurable: true }); },
    (a, step) => { if (step === 0) a[50000] = "sparse"; if (step === 1) a.length = 3; },
    (a, step) => { if (step === 0) Object.freeze(a); },
    (a, step) => { if (step === 0) Object.setPrototypeOf(a, { __proto__: Array.prototype, 2: "fromProto" }); if (step === 1) delete a[2]; },
];

for (let round = 0; round < 2; round++) {
    for (let m = 0; m < makers.length; m++) {
        for (let k = 0; k < mutations.length; k++) {
            let mutate = guarded(mutations[k]);
            shouldBe(viaForOf(makers[m](), mutate), viaIterator(makers[m](), mutate), "for-of, array " + m + ", mutation " + k);
            if (m !== 8)
                shouldBe(viaDestructuring(makers[m](), mutate), viaDestructuringByHand(makers[m](), mutate), "destructuring, array " + m + ", mutation " + k);
        }
    }
}

// Holes read through Array.prototype and Object.prototype, including accessors installed while the loop runs.
{
    function sumWithHoles(array) { let s = ""; for (let x of array) s += show(x) + ";"; return s; }
    for (let i = 0; i < testLoopCount; i++)
        shouldBe(sumWithHoles([1, , 3]), "1;u;3;");
    Array.prototype[1] = "AP";
    shouldBe(sumWithHoles([1, , 3]), "1;AP;3;");
    delete Array.prototype[1];
    Object.prototype[1] = "OP";
    shouldBe(sumWithHoles([1, , 3]), "1;OP;3;");
    delete Object.prototype[1];
    let calls = 0;
    Object.defineProperty(Array.prototype, 1, { get() { calls++; this.push("grown" + calls); return "G" + calls; }, configurable: true });
    shouldBe(sumWithHoles([1, , 3]), "1;G1;3;grown1;");
    shouldBe(calls, 1);
    delete Array.prototype[1];
    for (let i = 0; i < testLoopCount; i++)
        shouldBe(sumWithHoles([1, , 3]), "1;u;3;");
}

// A getter that throws: the loop ends, nothing to close (the spec does not call return when next throws), and the
// exception is the getter's.
{
    function throwingGetter(array) { let seen = []; try { for (let x of array) seen.push(x); } catch (e) { seen.push("caught " + e.message); } return seen.join(); }
    function make() { let a = [1, 2, 3]; Object.defineProperty(a, 1, { get() { throw new Error("from getter"); } }); return a; }
    for (let i = 0; i < testLoopCount; i++)
        shouldBe(throwingGetter(make()), "1,caught from getter");
}
