function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

let counter = 0;
let phase = 0;
let log = [];
function next() {
    ++counter;
    let result = (phase && !(counter % 97)) ? 0.5 : (counter & 0xff);
    log.push(result);
    return result;
}
noInline(next);

function* sameBlock() {
    let v = next();
    let w = next();
    let sum = 0;
    while (true) {
        yield sum;
        sum = v + w;
        v = next();
        w = next();
    }
}

function* acrossBranch(v, flag) {
    let x = v;
    let r = 0;
    if (flag) {
        yield 1;
        r = x < 10 ? 1 : 2;
    } else
        x = "s";
    yield r;
    return x;
}

function drive(v, flag) {
    let iterator = acrossBranch(v, flag);
    if (flag)
        shouldBe(iterator.next().value, 1);
    let r = iterator.next().value;
    let result = iterator.next();
    shouldBe(result.done, true);
    shouldBe(result.value, flag ? v : "s");
    return r;
}
noInline(drive);

function* returnsRestored(v) {
    let x = v;
    yield 0;
    return x;
}

function useReturned(v) {
    let iterator = returnsRestored(v);
    iterator.next();
    return iterator.next().value + 1;
}
noInline(useReturned);

function* consecutive(v) {
    let x = v;
    yield 0;
    yield 1;
    return x;
}

function useConsecutive(v) {
    let iterator = consecutive(v);
    iterator.next();
    iterator.next();
    iterator.next();
    return iterator.next().done;
}
noInline(useConsecutive);

async function asyncLocals(v) {
    let a = v;
    let b = v + 1;
    await null;
    return a + b;
}

let iterator = sameBlock();
shouldBe(iterator.next().value, 0);
for (let i = 0; i < testLoopCount * 2; ++i) {
    if (i === testLoopCount)
        phase = 1;
    shouldBe(iterator.next().value, log[2 * i] + log[2 * i + 1]);
}

for (let i = 0; i < testLoopCount * 2; ++i) {
    let v = (i > testLoopCount && (i % 97) === 96) ? 0.5 : 1;
    let flag = !!(i & 3);
    shouldBe(drive(v, flag), flag ? 1 : 0);
}

for (let i = 0; i < testLoopCount * 2; ++i) {
    let v = (i > testLoopCount && (i % 97) === 96) ? 0.5 : i;
    shouldBe(useReturned(v), v + 1);
    shouldBe(useConsecutive(v), true);
}

let done = 0;
for (let i = 0; i < testLoopCount * 2; ++i) {
    let v = (i > testLoopCount && (i % 97) === 96) ? 0.5 : i;
    asyncLocals(v).then((result) => {
        shouldBe(result, v + v + 1);
        ++done;
    });
}
drainMicrotasks();
shouldBe(done, testLoopCount * 2);
