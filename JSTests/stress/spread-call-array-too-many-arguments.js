//@ $skipModes << :lockdown

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} expected: ${String(expected)}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}, got ${String(error)}`);
}

function count() { return arguments.length; }
noInline(count);
function countInlined(a, b) { return arguments.length; }

function call(array) { return count(...array); }
noInline(call);
function callInlined(array) { return countInlined(...array); }
noInline(callInlined);
function apply(array) { return count.apply(undefined, array); }
noInline(apply);
function callAndCatch(array, fallback) {
    try {
        return count(...array);
    } catch (error) {
        return fallback + array.length;
    }
}
noInline(callAndCatch);
function applyAndCatch(array, fallback) {
    try {
        return count.apply(undefined, array);
    } catch (error) {
        return fallback + array.length;
    }
}
noInline(applyAndCatch);

const small = [1, 2, 3];
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(call(small), 3);
    shouldBe(callInlined(small), 3);
    shouldBe(apply(small), 3);
    shouldBe(callAndCatch(small, 1), 3);
    shouldBe(applyAndCatch(small, 1), 3);
}

const large = new Array(50000).fill(1);
const largeDoubles = new Array(50000).fill(1.5);
const tooLarge = new Array(0x100000 + 1).fill(1);
for (let i = 0; i < 10; ++i) {
    shouldBe(call(large), 50000);
    shouldBe(callInlined(large), 50000);
    shouldBe(apply(large), 50000);
    shouldBe(call(largeDoubles), 50000);
    shouldThrow(() => call(tooLarge), RangeError);
    shouldThrow(() => callInlined(tooLarge), RangeError);
    shouldThrow(() => apply(tooLarge), RangeError);
    shouldBe(callAndCatch(tooLarge, 1), 0x100000 + 2);
    shouldBe(applyAndCatch(tooLarge, 1), 0x100000 + 2);
}

function recurse(array) { return recurse(...[array]) + 1; }
shouldThrow(() => recurse(small), RangeError);
