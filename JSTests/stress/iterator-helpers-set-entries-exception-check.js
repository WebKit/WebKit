//@ runDefault("--validateExceptionChecks=1")

// Regression test for the Set-iterator fast path in forEachInIteratorProtocol.
// JSSetIterator::next() can throw (constructArrayPair can OOM when kind is
// Entries), so the fast path must check for an exception after next() before
// invoking the user callback. With --validateExceptionChecks=1, a missing
// check fires an "exception check validation failed" assertion on debug/ASan
// builds when the callback (a JS function) is called.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

// Set entries iterator + Iterator.prototype.forEach (JS callback).
{
    const set = new Set([1, 2, 3]);
    const seen = [];
    set.entries().forEach((entry) => {
        seen.push(entry[0], entry[1]);
    });
    shouldBe(seen.join(","), "1,1,2,2,3,3");
}

// Same, with a non-JS (bound) callback to cover the non-CachedCall path.
{
    const set = new Set(["a", "b"]);
    const seen = [];
    const callback = function (entry) { seen.push(this.prefix + entry[0] + entry[1]); }.bind({ prefix: "x" });
    set.entries().forEach(callback);
    shouldBe(seen.join(","), "xaa,xbb");
}

// Set entries iterator + Iterator.prototype.toArray.
{
    const set = new Set([1, 2, 3]);
    const result = set.entries().toArray();
    shouldBe(JSON.stringify(result), "[[1,1],[2,2],[3,3]]");
}

// Map entries iterator for symmetry (this path already had the check).
{
    const map = new Map([[1, "a"], [2, "b"]]);
    const seen = [];
    map.entries().forEach((entry) => {
        seen.push(entry[0], entry[1]);
    });
    shouldBe(seen.join(","), "1,a,2,b");
}

// Repeat to make sure JIT tiers behave the same.
for (let i = 0; i < 1e3; ++i) {
    const set = new Set([i, i + 1]);
    const seen = [];
    set.entries().forEach((entry) => {
        seen.push(entry[0], entry[1]);
    });
    shouldBe(seen.join(","), `${i},${i},${i + 1},${i + 1}`);
}
