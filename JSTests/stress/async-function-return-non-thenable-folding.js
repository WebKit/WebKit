// Verifies observable behavior of async functions without await when constant
// folding proves the returned value is not a thenable. All promise resolution
// semantics (thenable adoption, "then" getter observability, microtask ordering,
// watchpoint invalidation) must be unchanged.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "bad value: " + actual + ", expected: " + expected);
}

const log = [];

// Phase 1: plain object return -- value identity, async fulfillment.
{
    async function toDTO(row) {
        return { id: row.id, name: row.first, score: row.score, ok: true };
    }
    const rows = [];
    for (let j = 0; j < 16; j++)
        rows.push({ id: j, first: 'A' + j, last: 'B' + j, score: j * 3 });

    let sum = 0;
    for (let i = 0; i < 2e5; i++) {
        toDTO(rows[i & 15]).then((dto) => { sum += dto.score; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    let expected = 0;
    for (let i = 0; i < 2e5; i++)
        expected += (i & 15) * 3;
    shouldBe(sum, expected, "phase1 sum");

    // Fulfillment must be asynchronous even when fast-pathed.
    let syncCheck = "pending";
    toDTO(rows[0]).then(() => { syncCheck = "fulfilled"; });
    shouldBe(syncCheck, "pending", "phase1 no sync fulfillment");
    drainMicrotasks();
    shouldBe(syncCheck, "fulfilled", "phase1 fulfilled after drain");
    log.push("phase1 ok");
}

// Phase 2: primitive returns.
{
    async function compute(x) {
        return x * 2 + 1;
    }
    let last = 0;
    for (let i = 0; i < 2e5; i++)
        compute(i).then((v) => { last = v; });
    drainMicrotasks();
    shouldBe(last, (2e5 - 1) * 2 + 1, "phase2 last");
    log.push("phase2 ok");
}

// Phase 3: thenable with own "then" method -- must be adopted, called once per resolve.
{
    let thenCalls = 0;
    async function makeThenable(i) {
        return { then(resolve, reject) { thenCalls++; resolve(i * 10); } };
    }
    let got = -1;
    for (let i = 0; i < 2e5; i++) {
        makeThenable(i).then((v) => { got = v; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(thenCalls, 2e5, "phase3 then call count");
    shouldBe(got, (2e5 - 1) * 10, "phase3 adopted value");
    log.push("phase3 ok");
}

// Phase 4: "then" accessor on the prototype -- the Get must remain observable,
// exactly once per resolution, with the resolved object as receiver.
{
    let getterCalls = 0;
    let lastReceiverId = -1;
    class DTOWithAccessor {
        constructor(id) { this.id = id; }
        get then() { getterCalls++; lastReceiverId = this.id; return undefined; }
    }
    async function makeAccessorDTO(i) {
        return new DTOWithAccessor(i);
    }
    let gotId = -1;
    for (let i = 0; i < 2e5; i++) {
        makeAccessorDTO(i).then((v) => { gotId = v.id; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(getterCalls, 2e5, "phase4 getter call count");
    shouldBe(lastReceiverId, 2e5 - 1, "phase4 receiver");
    shouldBe(gotId, 2e5 - 1, "phase4 fulfilled with object");
    log.push("phase4 ok");
}

// Phase 5: "then" getter that throws -- promise must reject.
{
    const poison = {};
    Object.defineProperty(poison, "then", { get() { throw new Error("poisoned"); } });
    async function returnPoison() {
        return poison;
    }
    let rejected = null;
    returnPoison().then(() => { rejected = "fulfilled"; }, (e) => { rejected = e.message; });
    drainMicrotasks();
    shouldBe(rejected, "poisoned", "phase5 rejection");
    log.push("phase5 ok");
}

// Phase 6: returning a promise -- must create a fresh promise that adopts it.
{
    async function passThrough(p) {
        return p;
    }
    const inner = Promise.resolve(42);
    const outer = passThrough(inner);
    shouldBe(outer === inner, false, "phase6 fresh promise");
    let got = -1;
    outer.then((v) => { got = v; });
    drainMicrotasks();
    shouldBe(got, 42, "phase6 adopted value");

    // Microtask tick ordering when returning a promise must be unchanged.
    const order = [];
    async function g() { return Promise.resolve("g"); }
    g().then((v) => order.push(v));
    Promise.resolve().then(() => order.push("t1")).then(() => order.push("t2")).then(() => order.push("t3")).then(() => order.push("t4"));
    drainMicrotasks();
    log.push("phase6 order: " + order.join(","));
}

// Phase 7: microtask tick ordering for plain object return must be unchanged.
{
    const order = [];
    async function h() { return { v: "h" }; }
    h().then((o) => order.push(o.v));
    Promise.resolve().then(() => order.push("t1")).then(() => order.push("t2")).then(() => order.push("t3"));
    drainMicrotasks();
    log.push("phase7 order: " + order.join(","));
}

// Phase 8: polymorphic DTO shapes.
{
    async function poly(o) {
        return o.flip ? { a: o.id, kind: "x" } : { b: o.id, kind: "y", extra: 1 };
    }
    let kinds = { x: 0, y: 0 };
    for (let i = 0; i < 2e5; i++) {
        poly({ id: i, flip: !!(i & 1) }).then((r) => { kinds[r.kind]++; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(kinds.x, 1e5, "phase8 x count");
    shouldBe(kinds.y, 1e5, "phase8 y count");
    log.push("phase8 ok");
}

// Phase 9: adding "then" to Object.prototype AFTER tier-up must invalidate the
// proof and route resolution through the thenable path.
{
    async function makePlain(i) {
        return { id: i };
    }
    let sum = 0;
    for (let i = 0; i < 2e5; i++) {
        makePlain(i).then((o) => { sum += (typeof o === "object") ? 1 : 0; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(sum, 2e5, "phase9 warmup");

    Object.prototype.then = function (resolve, reject) { resolve("hijacked:" + this.id); };
    let got = null;
    makePlain(7).then((v) => { got = v; });
    drainMicrotasks();
    shouldBe(got, "hijacked:7", "phase9 hijacked resolution");

    delete Object.prototype.then;
    let got2 = null;
    makePlain(8).then((v) => { got2 = v.id; });
    drainMicrotasks();
    shouldBe(got2, 8, "phase9 restored resolution");
    log.push("phase9 ok");
}

// Phase 10: adding "then" to a custom prototype AFTER tier-up.
{
    class DTO {
        constructor(id) { this.id = id; }
    }
    async function makeDTO(i) {
        return new DTO(i);
    }
    let count = 0;
    for (let i = 0; i < 2e5; i++) {
        makeDTO(i).then(() => { count++; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(count, 2e5, "phase10 warmup");

    DTO.prototype.then = function (resolve) { resolve("proto-then:" + this.id); };
    let got = null;
    makeDTO(9).then((v) => { got = v; });
    drainMicrotasks();
    shouldBe(got, "proto-then:9", "phase10 proto then");
    log.push("phase10 ok");
}

// Phase 11: async arrow and async method forms.
{
    const toDTOArrow = async (row) => ({ id: row.id, double: row.id * 2 });
    const service = {
        async toDTO(row) { return { id: row.id, triple: row.id * 3 }; }
    };
    let a = -1, m = -1;
    for (let i = 0; i < 1e5; i++) {
        toDTOArrow({ id: i }).then((r) => { a = r.double; });
        service.toDTO({ id: i }).then((r) => { m = r.triple; });
        if ((i & 0xfff) === 0)
            drainMicrotasks();
    }
    drainMicrotasks();
    shouldBe(a, (1e5 - 1) * 2, "phase11 arrow");
    shouldBe(m, (1e5 - 1) * 3, "phase11 method");
    log.push("phase11 ok");
}

print(log.join("\n"));
