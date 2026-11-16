// A cross-realm async iterator must never take the same-realm cooperative fast driver: the fast path is gated
// by @@asyncIterator / next identity against the current realm's link-time constants, so a foreign async
// generator (or foreign sync iterable) always drives through the generic path. Verify correctness and that a
// foreign generator's internally-awaited thenable resolves in the generator's realm. Warmed to reach upper tiers.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

let done = 0;
let error = null;

const foreignConsumedInMain = createGlobalObject();
const makeForeignGen = new foreignConsumedInMain.Function("return (async function*(){ yield 1; yield 2; yield 3; });")();
const makeForeignArray = new foreignConsumedInMain.Function("return function(){ return [7, 8, 9]; };")();

// A foreign async generator that yields a thenable; its internal Await must resolve in the generator's realm.
const thenableRealmProbe = createGlobalObject();
thenableRealmProbe.MainFunction = Function;
thenableRealmProbe.record = (label) => { error = error || (label === "gen-realm" ? null : new Error("thenable resolved in " + label)); };
const makeThenableGen = new thenableRealmProbe.Function(`
    return (async function*(){ yield { then(f) {
        record(f.constructor === Function ? "gen-realm" : f.constructor === MainFunction ? "main-realm" : "unknown");
        f(0);
    } }; });
`)();

// A main-realm async generator consumed by a foreign-realm for-await driver.
const mainConsumedInForeign = createGlobalObject();
mainConsumedInForeign.makeMainGen = () => (async function*(){ yield 10; yield 20; yield 30; })();
mainConsumedInForeign.assert = assert;
mainConsumedInForeign.finish = () => { done++; };

async function main() {
    const N = testLoopCount;
    for (let i = 0; i < N; i++) {
        let seq = [];
        for await (const x of makeForeignGen())
            seq.push(x);
        assert(seq.join(",") === "1,2,3", "foreign async generator in main realm");

        seq = [];
        for await (const x of makeForeignArray())
            seq.push(x);
        assert(seq.join(",") === "7,8,9", "foreign sync iterable in main realm");
    }

    for await (const _ of makeThenableGen()) { }
    done++;
}

new mainConsumedInForeign.Function(`
    (async () => {
        for (let i = 0; i < 100; i++) {
            let seq = [];
            for await (const x of makeMainGen())
                seq.push(x);
            assert(seq.join(",") === "10,20,30", "main async generator in foreign realm");
        }
        finish();
    })();
`)();

main().then(() => { done++; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done === 3, "all cross-realm drivers completed, got " + done);
