// https://bugs.webkit.org/show_bug.cgi?id=316447
// %AsyncGeneratorPrototype%.next step 5: completed -> resolve { undefined, true } directly. Resolving reads
// `then`, so a reentrant next() must still see the completed state and resolve inline (h2 before m1).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

let log = [];

async function* gen() { }
let it = gen();

it.next().then(function () {
    let phase = 1;
    Object.defineProperty(Object.prototype, "then", {
        configurable: true,
        get() {
            if (phase === 1) {
                phase = 2;
                it.next().then(() => { log.push("h2"); });
                Promise.resolve().then(() => { log.push("m1"); });
            }
            return undefined;
        },
    });

    it.next();
});

drainMicrotasks();

shouldBe(log.join("|"), "h2|m1");
