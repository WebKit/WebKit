function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

// The loops below see an Array, which they iterate without an iterator object, and an iterable whose iterator is a generator
// created by code that gets inlined, so its structure is known. The next and return methods of generators have seen generators
// of more than one structure, so they convert this when they get inlined too.

function* first() { yield 1; yield 2; yield 3; }
function* second() { yield 1; yield 2; yield 3; }
const withGenerator = { [Symbol.iterator]: first };

function collect(iterable) { let seen = []; for (let x of iterable) seen.push(x); return seen.join(); }
function breakAtTwo(iterable) { let seen = []; for (let x of iterable) { seen.push(x); if (x === 2) break; } return seen.join(); }
function firstTwo(iterable) { let [a, b] = iterable; return a + "," + b; }
noInline(collect); noInline(breakAtTwo); noInline(firstTwo);

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(collect(second()), "1,2,3");
    shouldBe(breakAtTwo(second()), "1,2");
    for (let iterable of [[1, 2, 3], withGenerator]) {
        shouldBe(collect(iterable), "1,2,3");
        shouldBe(breakAtTwo(iterable), "1,2");
        shouldBe(firstTwo(iterable), "1,2");
    }
}
