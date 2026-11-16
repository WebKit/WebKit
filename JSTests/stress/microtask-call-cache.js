// Exercises the microtask call cache in the drain loop: more distinct callees
// than cache entries, closures sharing one executable, and a handler whose
// arity exceeds the fast path limit.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

async function a1() { return 1; }
async function a2() { return await a1() + 1; }
async function a3() { return await a2() + 1; }
async function a4() { return await a3() + 1; }
async function a5() { return await a4() + 1; }
async function a6() { return await a5() + 1; }
async function a7() { return await a6() + 1; }
async function a8() { return await a7() + 1; }
async function a9() { return await a8() + 1; }
async function a10() { return await a9() + 1; }
async function a11() { return await a10() + 1; }
async function a12() { return await a11() + 1; }

function makeAdder(n) {
    return async function adder() { return n + await a1(); };
}
const add10 = makeAdder(10);
const add20 = makeAdder(20);
const add30 = makeAdder(30);

const wideArity = (a, b, c, d, e, f, g, h) => a + 100;

async function test() {
    shouldBe(await a12(), 12);
    shouldBe(await add10(), 11);
    shouldBe(await add20(), 21);
    shouldBe(await add30(), 31);
    shouldBe(await Promise.resolve(1).then(wideArity), 101);
    shouldBe(await Promise.resolve(2).then(wideArity).then(add10), 11);
}

async function run() {
    for (let i = 0; i < testLoopCount; i++)
        await test();
}

let finished = false;
let error;
run().then(() => { finished = true; }, (e) => { error = e; });
drainMicrotasks();
if (error)
    throw error;
if (!finished)
    throw new Error("did not finish");
