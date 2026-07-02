function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

// Each scenario uses its own noInline helper so the PromiseResolve call site stays
// monomorphic and the intended ConstantFoldingPhase branch is actually exercised.

// Branch 3: plain non-thenable object with a single watched structure -> folds to
// NewResolvedPromise(isResolvedValueKnownNonThenable), inline-allocated fulfilled.
function resolveObject(o) {
    return Promise.resolve(o);
}
noInline(resolveObject);

// Branch 1: non-object argument -> trivially non-thenable.
function resolveValue(v) {
    return Promise.resolve(v);
}
noInline(resolveValue);

// `then` present -> tryEnsureAbsence fails, no fold, object must be adopted.
function resolveThenable(o) {
    return Promise.resolve(o);
}
noInline(resolveThenable);

// Non-default constructor -> not folded, goes through the species path.
class MyPromise extends Promise { }
function resolveSubclass(o) {
    return Promise.resolve.call(MyPromise, o);
}
noInline(resolveSubclass);

// Dedicated monomorphic site for the watchpoint-invalidation scenario.
function Proto() { }
function resolveProto(o) {
    return Promise.resolve(o);
}
noInline(resolveProto);

async function main() {
    // 1. Plain object: fulfilled with the object itself, distinct promise cell.
    for (let i = 0; i < testLoopCount; ++i) {
        let o = { a: i, b: i + 1 };
        let p = resolveObject(o);
        shouldBe(p instanceof Promise, true);
        shouldBe(p !== o, true);
        shouldBe(await p, o);
    }

    // 2. .then() handler receives the object; chaining and microtask ordering.
    for (let i = 0; i < testLoopCount; ++i) {
        let o = { x: i };
        let order = [];
        let q = resolveObject(o).then(v => { order.push("then"); return v.x; });
        order.push("sync");
        shouldBe(await q, i);
        shouldBe(order.join(","), "sync,then");
    }

    // 3. Branch 1: non-object values are non-thenable.
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBe(await resolveValue(i), i);
        shouldBe(await resolveValue(undefined), undefined);
        shouldBe(await resolveValue("s" + (i & 7)), "s" + (i & 7));
        shouldBe(await resolveValue(null), null);
        shouldBe(await resolveValue(i & 1 ? true : false), !!(i & 1));
    }

    // 4. GC stress: the inline-allocated promise must keep its slot value alive.
    {
        let promises = [];
        let objects = [];
        for (let i = 0; i < testLoopCount; ++i) {
            let o = { g: i };
            objects.push(o);
            promises.push(resolveObject(o));
        }
        gc();
        for (let i = 0; i < promises.length; ++i) {
            shouldBe(promises[i] !== objects[i], true);
            shouldBe(await promises[i], objects[i]);
        }
    }

    // 5. Subclass constructor: not folded, still correct, produces a MyPromise.
    for (let i = 0; i < testLoopCount; ++i) {
        let o = { s: i };
        let p = resolveSubclass(o);
        shouldBe(p instanceof MyPromise, true);
        shouldBe(await p, o);
    }

    // 6. `then` as an own property: object IS thenable, must be adopted (no fold).
    for (let i = 0; i < testLoopCount; ++i) {
        let adopted = false;
        let o = { then(resolve) { adopted = true; resolve("own-then-" + (i & 15)); } };
        shouldBe(await resolveThenable(o), "own-then-" + (i & 15));
        shouldBe(adopted, true);
    }

    // 7. Watchpoint invalidation: after tiering up on a non-thenable structure,
    //    adding a callable `then` to the prototype must invalidate the fast path
    //    so later calls adopt the now-thenable object instead of fulfilling with it.
    for (let i = 0; i < testLoopCount; ++i) {
        let o = new Proto();
        shouldBe(await resolveProto(o), o);
    }
    Proto.prototype.then = function (resolve) { resolve("via-proto-then"); };
    for (let i = 0; i < testLoopCount; ++i) {
        let o = new Proto();
        shouldBe(await resolveProto(o), "via-proto-then");
    }
}

main().catch($vm.abort);
