function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: actual=${JSON.stringify(actual)} expected=${JSON.stringify(expected)}`);
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try { func(); } catch (e) {
        errorThrown = true;
        if (String(e) !== errorMessage)
            throw new Error(`bad error: ${e}`);
    }
    if (!errorThrown)
        throw new Error("not thrown");
}

const log = [];
function tick(label) { Promise.resolve().then(() => log.push(label)); }

async function main() {
    // Promise.all over primitives, non-thenable objects, and promises.
    {
        const obj = { a: 1 };
        const p = Promise.resolve("p");
        const result = await Promise.all([1, "two", obj, p, null, undefined, 3.5, true, Symbol.for("s"), 4n]);
        shouldBe(result.length, 10);
        shouldBe(result[0], 1);
        shouldBe(result[1], "two");
        shouldBe(result[2], obj);
        shouldBe(result[3], "p");
        shouldBe(result[4], null);
        shouldBe(result[5], undefined);
        shouldBe(result[6], 3.5);
        shouldBe(result[7], true);
        shouldBe(result[8], Symbol.for("s"));
        shouldBe(result[9], 4n);
    }

    // Microtask ordering must be unchanged: each non-thenable element still
    // burns exactly one tick before the outer promise resolves, so the "all"
    // callback (queued by the 3rd element job) drains after the 4 ticks that
    // were already queued at that point.
    {
        log.length = 0;
        Promise.all([1, 2, 3]).then(() => log.push("all"));
        tick("a"); tick("b"); tick("c"); tick("d");
        for (var i = 0; i < 8; ++i)
            await null;
        shouldBe(JSON.stringify(log), '["a","b","c","d","all"]');
    }

    // Promise.allSettled
    {
        const obj = { foo: 1 };
        const result = await Promise.allSettled([1, obj, Promise.reject("err"), null]);
        shouldBe(result.length, 4);
        shouldBe(result[0].status, "fulfilled");
        shouldBe(result[0].value, 1);
        shouldBe(result[1].status, "fulfilled");
        shouldBe(result[1].value, obj);
        shouldBe(result[2].status, "rejected");
        shouldBe(result[2].reason, "err");
        shouldBe(result[3].status, "fulfilled");
        shouldBe(result[3].value, null);
    }

    // Promise.any — all rejecting except one non-thenable.
    {
        const result = await Promise.any([Promise.reject("a"), 42, Promise.reject("b")]);
        shouldBe(result, 42);
    }
    {
        let aggregate;
        try {
            await Promise.any([Promise.reject("x"), Promise.reject("y")]);
        } catch (e) { aggregate = e; }
        shouldBe(aggregate instanceof AggregateError, true);
    }

    // Promise.race — first non-thenable wins.
    {
        const result = await Promise.race([1, 2, Promise.resolve(3)]);
        shouldBe(result, 1);
    }
    {
        const never = new Promise(() => {});
        const result = await Promise.race([never, "fast"]);
        shouldBe(result, "fast");
    }

    // Thenable elements still go through Promise.resolve(thenable) machinery.
    {
        let calls = 0;
        const thenable = {
            then(resolve, reject) { calls++; resolve(99); }
        };
        const result = await Promise.all([1, thenable, 2]);
        shouldBe(JSON.stringify(result), '[1,99,2]');
        shouldBe(calls, 1);
    }

    // Object that gains `then` via Object.prototype.then must not take the
    // non-thenable fast path: the watchpoint must drop us into the
    // Promise.resolve(thenable) machinery for plain objects.
    {
        const plain = { plain: true };
        let plainObjectAdopted = false;
        Object.prototype.then = function (resolve) {
            if (this === plain)
                plainObjectAdopted = true;
            resolve("hijacked");
        };
        try {
            // Note: the result array also goes through the hijacked then.
            const result = await Promise.all([1, plain, 2]);
            shouldBe(result, "hijacked");
            shouldBe(plainObjectAdopted, true);
        } finally {
            delete Object.prototype.then;
        }
    }

    // A Structure cached as DefinitelyNonThenable before the watchpoint fires
    // must not be treated as non-thenable afterward.
    {
        class Box { constructor(value) { this.value = value; } }
        const result1 = await Promise.all([new Box(1), new Box(2)]);
        shouldBe(result1[0].value, 1);
        shouldBe(result1[1].value, 2);

        let boxAdopted = false;
        Object.prototype.then = function (resolve) {
            if (this instanceof Box)
                boxAdopted = true;
            resolve("hijacked");
        };
        try {
            await Promise.all([new Box(3)]);
            shouldBe(boxAdopted, true);
        } finally {
            delete Object.prototype.then;
        }
    }

    // Promise subclass with custom Symbol.species must not take the fast path.
    {
        class MyPromise extends Promise {
            static get [Symbol.species]() { return Promise; }
        }
        const result = await Promise.all([1, MyPromise.resolve(2), 3]);
        shouldBe(JSON.stringify(result), "[1,2,3]");
    }

    // A crafted Promise.prototype.then must be invoked per element, even for non-thenable ones.
    {
        const originalThen = Promise.prototype.then;
        const counted = (combinator, elements) => {
            let thenCalls = 0;
            Promise.prototype.then = function (onFulfilled, onRejected) {
                thenCalls++;
                return originalThen.call(this, onFulfilled, onRejected);
            };
            try {
                Promise[combinator](elements);
            } finally {
                Promise.prototype.then = originalThen;
            }
            return thenCalls;
        };
        shouldBe(counted("all", [1, "two", null]) >= 3, true);
        shouldBe(counted("allSettled", [1, "two", null]) >= 3, true);
        shouldBe(counted("any", [1, "two", null]) >= 3, true);
        shouldBe(counted("race", [1, "two", null]) >= 3, true);

        Promise.prototype.then = function (onFulfilled, onRejected) {
            return originalThen.call(this, onFulfilled, onRejected);
        };
        try {
            const result = await Promise.all([1, "two", null, true]);
            shouldBe(JSON.stringify(result), '[1,"two",null,true]');
        } finally {
            Promise.prototype.then = originalThen;
        }
    }

    // Empty iterable.
    {
        const result = await Promise.all([]);
        shouldBe(result.length, 0);
    }
}

let failed = null;
main().catch((e) => { failed = e; });
drainMicrotasks();
if (failed)
    throw failed;
