// Verifies that async functions without await still take the thenable path when
// the returned object has a "then": as an own property, only on the prototype,
// deeper in the prototype chain, added after tier-up, or non-callable. The
// non-thenable constant folding proof must never fire for these shapes.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "bad value: " + actual + ", expected: " + expected);
}

const ITERS = 2e5;

// Own "then" data property in the object literal: adopted, called once per resolve.
{
    let thenCalls = 0;
    async function makeOwnThenable(i) {
        return {
            id: i,
            then(resolve, reject) {
                thenCalls++;
                shouldBe(this.id >= 0, true, "own receiver");
                resolve(this.id * 2);
            }
        };
    }
    let got = -1;
    for (let i = 0; i < ITERS; i++) {
        makeOwnThenable(i).then((v) => { got = v; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(thenCalls, ITERS, "own then call count");
    shouldBe(got, (ITERS - 1) * 2, "own then adopted value");
}

// "then" only on the prototype (class method), not an own property: adopted,
// looked up through the prototype with the object as receiver.
{
    let thenCalls = 0;
    class ProtoThenable {
        constructor(id) { this.id = id; }
        then(resolve, reject) {
            thenCalls++;
            resolve("proto:" + this.id);
        }
    }
    async function makeProtoThenable(i) {
        return new ProtoThenable(i);
    }
    shouldBe(Object.getOwnPropertyNames(new ProtoThenable(0)).includes("then"), false, "then is not own");
    let got = null;
    for (let i = 0; i < ITERS; i++) {
        makeProtoThenable(i).then((v) => { got = v; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(thenCalls, ITERS, "proto then call count");
    shouldBe(got, "proto:" + (ITERS - 1), "proto then adopted value");
}

// "then" two levels up the prototype chain.
{
    let thenCalls = 0;
    const grandproto = {
        then(resolve, reject) {
            thenCalls++;
            resolve("grand:" + this.id);
        }
    };
    const proto = Object.create(grandproto);
    async function makeDeepThenable(i) {
        const o = Object.create(proto);
        o.id = i;
        return o;
    }
    let got = null;
    for (let i = 0; i < ITERS; i++) {
        makeDeepThenable(i).then((v) => { got = v; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(thenCalls, ITERS, "grandproto then call count");
    shouldBe(got, "grand:" + (ITERS - 1), "grandproto then adopted value");
}

// "then" getter on the prototype returning a callable: the getter must run once
// per resolution and the returned function must be invoked.
{
    let getterCalls = 0;
    let thenCalls = 0;
    class GetterThenable {
        constructor(id) { this.id = id; }
        get then() {
            getterCalls++;
            const self = this;
            return function (resolve, reject) {
                thenCalls++;
                resolve("getter:" + self.id);
            };
        }
    }
    async function makeGetterThenable(i) {
        return new GetterThenable(i);
    }
    let got = null;
    for (let i = 0; i < ITERS; i++) {
        makeGetterThenable(i).then((v) => { got = v; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(getterCalls, ITERS, "getter call count");
    shouldBe(thenCalls, ITERS, "returned then call count");
    shouldBe(got, "getter:" + (ITERS - 1), "getter then adopted value");
}

// Own "then" added conditionally after object creation. The branch is never
// taken during warmup, then flipped after tier-up: the structure transition
// must defeat any non-thenable proof.
{
    let thenCalls = 0;
    async function maybeThenable(i, flag) {
        const o = { id: i };
        if (flag)
            o.then = function (resolve, reject) { thenCalls++; resolve("late-own:" + this.id); };
        return o;
    }
    let plain = 0;
    for (let i = 0; i < ITERS; i++) {
        maybeThenable(i, false).then((o) => { plain += (o.id === i | 0); });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(plain, ITERS, "warmup without then");
    shouldBe(thenCalls, 0, "no then calls during warmup");

    let got = null;
    maybeThenable(123, true).then((v) => { got = v; });
    drainMicrotasks();
    shouldBe(thenCalls, 1, "late own then called");
    shouldBe(got, "late-own:123", "late own then adopted value");
}

// "then" added to the prototype AFTER tier-up: the absence watchpoint must fire
// and subsequent resolutions must adopt.
{
    let thenCalls = 0;
    class LateProto {
        constructor(id) { this.id = id; }
    }
    async function makeLateProto(i) {
        return new LateProto(i);
    }
    let count = 0;
    for (let i = 0; i < ITERS; i++) {
        makeLateProto(i).then(() => { count++; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(count, ITERS, "warmup before proto then");

    LateProto.prototype.then = function (resolve, reject) { thenCalls++; resolve("late-proto:" + this.id); };
    let got = null;
    for (let i = 0; i < 100; i++)
        makeLateProto(i).then((v) => { got = v; });
    drainMicrotasks();
    shouldBe(thenCalls, 100, "late proto then call count");
    shouldBe(got, "late-proto:99", "late proto then adopted value");
}

// Non-callable "then" on the prototype: NOT a thenable, the promise must
// fulfill with the object itself.
{
    class NonCallableThen {
        constructor(id) { this.id = id; }
    }
    NonCallableThen.prototype.then = 42;
    async function makeNonCallable(i) {
        return new NonCallableThen(i);
    }
    let gotId = -1;
    for (let i = 0; i < ITERS; i++) {
        makeNonCallable(i).then((o) => { gotId = o.id; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(gotId, ITERS - 1, "non-callable then fulfills with object");
}
