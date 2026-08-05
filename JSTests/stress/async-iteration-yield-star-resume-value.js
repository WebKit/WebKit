// yield* in async generators must forward the resume value to the delegated iterator's
// next(value)/throw(value)/return(value), and expose the delegated iterator's return value.
// It must NOT change `for await` (which calls next() with zero arguments). Covers the fast
// async-generator driver path, the generic async-iterator path, and the async-from-sync path.

function assert(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message || "assert"}: expected ${expected} but got ${actual}`);
}

// 1. Driver path: delegate to a genuine async generator, forward resume values, expose return value.
async function driverPath() {
    async function* inner() {
        const a = yield "i0";
        const b = yield "i1:" + a;
        return "ret:" + b;
    }
    async function* outer() {
        const r = yield* inner();
        yield "o:" + r;
    }
    const it = outer();
    assert((await it.next()).value, "i0", "driver step0");
    assert((await it.next("A")).value, "i1:A", "driver step1 forwards A");
    assert((await it.next("B")).value, "o:ret:B", "driver return value forwards B");
    assert((await it.next()).done, true, "driver done");
}

// 2. Generic async iterator: next/throw/return receive the forwarded value with exactly one argument.
async function genericPath() {
    let events = [];
    function makeIterable() {
        let i = 0;
        return { [Symbol.asyncIterator]() {
            return {
                next(v) { events.push(`next(${JSON.stringify(v)})#${arguments.length}`); return Promise.resolve(i < 2 ? { value: "v" + (i++), done: false } : { value: "fin", done: true }); },
                return(v) { events.push(`return(${JSON.stringify(v)})#${arguments.length}`); return Promise.resolve({ value: v, done: true }); },
                throw(v) { events.push(`throw(${JSON.stringify(v)})#${arguments.length}`); return Promise.resolve({ value: "handled", done: true }); },
            };
        } };
    }

    async function* outer() { return yield* makeIterable(); }
    let it = outer();
    assert((await it.next()).value, "v0", "generic step0");        // next(undefined)
    assert((await it.next("X")).value, "v1", "generic step1");      // next("X")
    assert((await it.next("Y")).value, "fin", "generic return");    // next("Y") -> done, value "fin"
    assert(events.join(","), 'next(undefined)#1,next("X")#1,next("Y")#1', "generic next arg forwarding");

    // throw() delegation forwards value with one argument.
    events = [];
    it = outer();
    await it.next();
    assert((await it.throw("E")).value, "handled", "generic throw result");
    assert(events.join(","), 'next(undefined)#1,throw("E")#1', "generic throw arg forwarding");

    // return() delegation forwards value with one argument.
    events = [];
    it = outer();
    await it.next();
    assert((await it.return("R")).value, "R", "generic return result");
    assert(events.join(","), 'next(undefined)#1,return("R")#1', "generic return arg forwarding");
}

// 3. `for await` must call next() with ZERO arguments (yield* change must not regress this).
async function forAwaitZeroArgs() {
    let argCounts = [];
    let i = 0;
    const iterable = { [Symbol.asyncIterator]() {
        return { next() { argCounts.push(arguments.length); return Promise.resolve(i < 3 ? { value: i++, done: false } : { value: undefined, done: true }); } };
    } };
    let sum = 0;
    for await (const x of iterable)
        sum += x;
    assert(sum, 3, "for-await sum");
    assert(argCounts.join(","), "0,0,0,0", "for-await calls next() with zero args");
}

// 4. async-from-sync: yield* over a sync iterable forwards the resume value to the sync next(value).
async function asyncFromSyncPath() {
    let events = [];
    function makeSyncIterable() {
        let i = 0;
        return { [Symbol.iterator]() {
            return { next(v) { events.push(`${JSON.stringify(v)}#${arguments.length}`); return i < 2 ? { value: "s" + (i++), done: false } : { value: "sfin", done: true }; } };
        } };
    }
    async function* outer() { return yield* makeSyncIterable(); }
    const it = outer();
    assert((await it.next()).value, "s0", "afs step0");
    assert((await it.next("P")).value, "s1", "afs step1");
    assert((await it.next("Q")).value, "sfin", "afs return");
    // AsyncFromSync forwards the argument to the underlying sync next().
    assert(events.join(","), 'undefined#1,"P"#1,"Q"#1', "async-from-sync arg forwarding");

    // Builtin array fast path values are correct.
    async function* outerArr() { return yield* [7, 8, 9]; }
    const it2 = outerArr();
    assert((await it2.next()).value, 7, "afs array 0");
    assert((await it2.next()).value, 8, "afs array 1");
    assert((await it2.next()).value, 9, "afs array 2");
    assert((await it2.next()).done, true, "afs array done");
}

async function main() {
    // Loop to encourage tier-up within each stress configuration.
    for (let i = 0; i < 200; ++i) {
        await driverPath();
        await genericPath();
        await forAwaitZeroArgs();
        await asyncFromSyncPath();
    }
}

let caught = null;
let completed = false;
main().then(() => { completed = true; }, e => { caught = e; });
drainMicrotasks();
if (caught)
    throw caught;
if (!completed)
    throw new Error("async work did not complete after draining microtasks");
