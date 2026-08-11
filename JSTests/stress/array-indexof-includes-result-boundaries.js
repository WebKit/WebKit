// indexOf/includes result materialization across element types and tiers. Covers the boundary
// cases for the branchless includes result (index != length): match at index 0, empty arrays
// (length 0), and fromIndex >= length.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

function i32Index(a, x) { return a.indexOf(x); }
noInline(i32Index);
function i32Includes(a, x) { return a.includes(x); }
noInline(i32Includes);
function i32IncludesFrom(a, x, from) { return a.includes(x, from); }
noInline(i32IncludesFrom);

function dblIndex(a, x) { return a.indexOf(x); }
noInline(dblIndex);
function dblIncludes(a, x) { return a.includes(x); }
noInline(dblIncludes);

function strIndex(a, x) { return a.indexOf(x); }
noInline(strIndex);
function strIncludes(a, x) { return a.includes(x); }
noInline(strIncludes);

const i32 = [10, 20, 30, 40];
const i32Empty = [];
const dbl = [1.5, 2.5, 3.5];
const dblEmpty = [];
const str = ['a', 'bb', 'ccc'];
const strEmpty = [];

for (let i = 0; i < testLoopCount; ++i) {
    // Match at index 0 must be found (guards against a naive "0 means not found" scheme).
    shouldBe(i32Index(i32, 10), 0);
    shouldBe(i32Includes(i32, 10), true);
    shouldBe(i32Index(i32, 40), 3);
    shouldBe(i32Includes(i32, 40), true);
    shouldBe(i32Index(i32, 99), -1);
    shouldBe(i32Includes(i32, 99), false);

    shouldBe(dblIndex(dbl, 1.5), 0);
    shouldBe(dblIncludes(dbl, 1.5), true);
    shouldBe(dblIndex(dbl, 9.9), -1);
    shouldBe(dblIncludes(dbl, 9.9), false);

    shouldBe(strIndex(str, 'a'), 0);
    shouldBe(strIncludes(str, 'a'), true);
    shouldBe(strIndex(str, 'zz'), -1);
    shouldBe(strIncludes(str, 'zz'), false);

    // Empty arrays (length 0): includes -> false, indexOf -> -1.
    shouldBe(i32Index(i32Empty, 5), -1);
    shouldBe(i32Includes(i32Empty, 5), false);
    shouldBe(dblIndex(dblEmpty, 1.5), -1);
    shouldBe(dblIncludes(dblEmpty, 1.5), false);
    shouldBe(strIndex(strEmpty, 'x'), -1);
    shouldBe(strIncludes(strEmpty, 'x'), false);

    // fromIndex >= length clamps to length, taking the not-found path.
    shouldBe(i32IncludesFrom(i32, 20, 5), false);
    shouldBe(i32IncludesFrom(i32, 10, 4), false);
    shouldBe(i32IncludesFrom(i32, 40, 3), true);
}
