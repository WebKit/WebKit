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

function prefix(x, array) { return count(x, ...array); }
noInline(prefix);
function two(array1, array2) { return count(...array1, ...array2); }
noInline(two);
function prefixAndCatch(x, array, fallback) {
    try {
        return count(x, ...array);
    } catch (error) {
        return fallback + array.length;
    }
}
noInline(prefixAndCatch);

var globalArray = [1];
function rest(...args) { return count(...args, ...globalArray); }
noInline(rest);

const small = [1, 2, 3];
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(prefix(0, small), 4);
    shouldBe(two(small, small), 6);
    shouldBe(prefixAndCatch(0, small, 1), 4);
    shouldBe(rest(1, 2), 3);
}

const large = new Array(50000).fill(1);
const largeDoubles = new Array(50000).fill(1.5);
const tooLarge = new Array(0x100000 + 1).fill(1);
const maximum = new Array(0x100000).fill(1);
const half = new Array(0x80000 + 1).fill(1);
for (let i = 0; i < 5; ++i) {
    shouldBe(prefix(0, large), 50001);
    shouldBe(prefix(0, largeDoubles), 50001);
    shouldBe(two(large, largeDoubles), 100000);
    shouldThrow(() => prefix(0, tooLarge), RangeError);
    shouldThrow(() => prefix(0, maximum), RangeError);
    shouldThrow(() => two(half, half), RangeError);
    shouldBe(prefixAndCatch(0, tooLarge, 1), 0x100000 + 2);
}

globalArray = large;
for (let i = 0; i < 5; ++i)
    shouldBe(rest(1, 2), 50002);
globalArray = maximum;
for (let i = 0; i < 5; ++i)
    shouldThrow(() => rest(1, 2), RangeError);
