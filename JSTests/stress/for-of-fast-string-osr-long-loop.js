// A loop over a String that runs without an iterator object changes tier in the middle.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function compiledReasonablyOften(f) {
    let n = numberOfDFGCompiles(f);
    if (n > 20 && n !== 1000000)
        throw new Error(f.name + " was compiled " + n + " times");
}

const N = testLoopCount;

function makeLong(n) { return "\u{1F600}aaaa".repeat(n / 5); }

// 1. One call, a long loop: enters the optimizing tiers in the middle of the loop.
function count(string) { let points = 0, units = 0; for (let x of string) { points++; units += x.length; } return points + "," + units; }
noInline(count);
shouldBe(count(makeLong(30 * N)), (30 * N) + "," + (36 * N));
shouldBe(count(makeLong(30 * N)), (30 * N) + "," + (36 * N));

// 2. break out of a loop that has been running optimized code for a while. The close has only seen Arrays before.
function find(iterable, wanted) { let n = 0; for (let x of iterable) { if (x === wanted) break; n++; } return n; }
noInline(find);
for (let i = 0; i < N; i++)
    shouldBe(find([1, 2, 3, 4], 3), 2);
shouldBe(find(makeLong(30 * N) + "z", "z"), 30 * N);
for (let i = 0; i < N; i++) {
    shouldBe(find("abcz", "z"), 3);
    shouldBe(find([1, 2, 3, 4], 3), 2);
}

// 3. Destructuring in a hot function.
function swap(pair) { let [a, b] = pair; return b + a; }
noInline(swap);
{
    let pair = "ab";
    for (let i = 0; i < 10 * N; i++)
        pair = swap(pair);
    shouldBe(pair, "ab");
    shouldBe(swap("\u{1F600}x"), "x\u{1F600}");
}

compiledReasonablyOften(count);
compiledReasonablyOften(find);
compiledReasonablyOften(swap);
