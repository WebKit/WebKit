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

function count(a, b) { return arguments.length; }

function call(array) { return count(...array); }
noInline(call);
function callAndCatch(array, fallback) {
    try {
        return count(...array);
    } catch (error) {
        return fallback + array.length;
    }
}
noInline(callAndCatch);

const small = [1, 2, 3];
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(call(small), 3);
    shouldBe(callAndCatch(small, 1), 3);
}

const large = new Array(50000).fill(1);
const largeDoubles = new Array(50000).fill(1.5);
const tooLarge = new Array(0x100000 + 1).fill(1);
for (let i = 0; i < 5; ++i) {
    shouldBe(call(large), 50000);
    shouldBe(call(largeDoubles), 50000);
    shouldThrow(() => call(tooLarge), RangeError);
    shouldBe(callAndCatch(tooLarge, 1), 0x100000 + 2);
}
