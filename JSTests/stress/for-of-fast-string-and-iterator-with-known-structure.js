function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

// The loops below see a String, which they iterate without an iterator object, and an iterable whose iterator is a generator
// created by code that gets inlined, so its structure is known. The next and return methods of generators have seen generators
// of more than one structure, so they convert this when they get inlined too.

function* first() { yield "a"; yield "b"; yield "c"; }
function* second() { yield "a"; yield "b"; yield "c"; }
const withGenerator = { [Symbol.iterator]: first };

function collect(iterable) { let seen = []; for (let x of iterable) seen.push(x); return seen.join(); }
function breakAtTwo(iterable) { let seen = []; for (let x of iterable) { seen.push(x); if (seen.length === 2) break; } return seen.join(); }
function firstTwo(iterable) { let [a, b] = iterable; return a + "," + b; }
noInline(collect); noInline(breakAtTwo); noInline(firstTwo);

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(collect(second()), "a,b,c");
    shouldBe(breakAtTwo(second()), "a,b");
    for (let iterable of ["abc", withGenerator]) {
        shouldBe(collect(iterable), "a,b,c");
        shouldBe(breakAtTwo(iterable), "a,b");
        shouldBe(firstTwo(iterable), "a,b");
    }
}
