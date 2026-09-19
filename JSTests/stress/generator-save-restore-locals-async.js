function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

async function asyncFunction(promise) {
    let a = 1, b = [1, 2, 3], c = "x";
    a += await promise;
    for (const v of b)
        c += await v;
    try {
        c += await Promise.reject("r");
    } catch (error) {
        c += error;
    } finally {
        c += await "f";
    }
    return a + c;
}

const asyncArrow = async (count) => {
    let sum = 0, product = 1;
    for (let i = 1; i <= count; ++i) {
        sum += await i;
        product *= await i;
    }
    return [sum, product];
};

class Klass {
    constructor() { this.value = 5; }
    async method(other) {
        let local = this.value;
        local += await other;
        return local + this.value;
    }
}

async function* asyncGenerator() {
    let accumulator = 0;
    for (let i = 0; i < 3; ++i) {
        accumulator += await i;
        yield accumulator;
    }
    yield* [10, 20];
    return accumulator;
}

function* sync() {
    let a = 1;
    a += yield a;
    return a;
}

let done = 0;
async function test() {
    shouldBe(await asyncFunction(Promise.resolve(10)), "11x123rf");
    shouldBe(JSON.stringify(await asyncArrow(5)), "[15,120]");
    shouldBe(await new Klass().method(Promise.resolve(3)), 13);
    let values = [];
    for await (const value of asyncGenerator())
        values.push(value);
    shouldBe(JSON.stringify(values), "[0,1,3,10,20]");
    let iterator = asyncGenerator();
    await iterator.next();
    shouldBe((await iterator.return("early")).value, "early");
    shouldBe((await iterator.next()).done, true);
    ++done;
}

for (let i = 0; i < testLoopCount; ++i)
    test();
drainMicrotasks();
shouldBe(done, testLoopCount);
