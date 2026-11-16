function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected ' + expected);
}

function ticksOf(body, makeSubject) {
    const log = [];
    const other = createGlobalObject();
    other.log = (s) => log.push(s);
    const { subject, settle } = makeSubject(other);
    other.subject = subject;
    new other.Function(body)();
    drainMicrotasks();
    settle();
    let p = Promise.resolve();
    for (let i = 0; i < 3; i++) {
        const j = i;
        p = p.then(() => log.push("c" + j));
    }
    drainMicrotasks();
    return log.join(" ");
}

function crossRealmPending() {
    let resolve;
    const promise = new Promise((r) => { resolve = r; });
    return { subject: promise, settle: () => { resolve(7); } };
}

function sameRealmPending(other) {
    new other.Function(`
        let r;
        globalThis.s = new Promise((res) => { r = res; });
        globalThis.doSettle = () => { r(7); };
    `)();
    return { subject: other.s, settle: () => { other.doSettle(); } };
}

const awaitBody = `(async () => { await subject; log("resumed"); })()`;

// Await performs PromiseResolve, whose SameValue(Get(x, "constructor"), C)
// check fails for a cross-realm promise: it must be re-wrapped through the
// thenable path, costing exactly one extra microtask compared to same-realm.
shouldBe(ticksOf(awaitBody, crossRealmPending), "c0 resumed c1 c2");
shouldBe(ticksOf(awaitBody, sameRealmPending), "resumed c0 c1 c2");

// Promise Resolve Functions have no such check: adopting a vanilla promise
// takes the same number of microtasks regardless of its realm.
const resolveBody = `
    const outer = new Promise((res) => { res(subject); });
    outer.then((v) => log("outer:" + v));
`;
shouldBe(ticksOf(resolveBody, crossRealmPending), "c0 outer:7 c1 c2");
shouldBe(ticksOf(resolveBody, sameRealmPending), "c0 outer:7 c1 c2");
