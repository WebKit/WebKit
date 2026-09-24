// A loop over an Array that runs without an iterator object changes tier in the middle: OSR entry from a long loop, OSR
// exit when a speculation fails half way, and the same code after IteratorClose became observable.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function compiledReasonablyOften(f) {
    let n = numberOfDFGCompiles(f);
    if (n > 20 && n !== 1000000)
        throw new Error(f.name + " was compiled " + n + " times");
}

const ArrayIteratorPrototype = Object.getPrototypeOf([][Symbol.iterator]());
const originalNext = ArrayIteratorPrototype.next;

const N = testLoopCount;

function makeLong(n, f) { let a = []; for (let i = 0; i < n; i++) a.push(f(i)); return a; }

// 1. One call, a long loop: enters the optimizing tiers in the middle of the loop.
function sumLong(array) { let s = 0; for (let x of array) s += x; return s; }
noInline(sumLong);
shouldBe(sumLong(makeLong(32 * N, i => i & 3)), 48 * N);
shouldBe(sumLong(makeLong(32 * N, i => i & 3)), 48 * N);

// 2. The element type changes half way: exit to the lower tiers in the middle of the loop, continue from the right index.
function joinLong(array) { let s = 0; let strings = 0; for (let x of array) { if (typeof x === "string") strings++; else s += x; } return s + ":" + strings; }
noInline(joinLong);
for (let i = 0; i < 5; i++)
    shouldBe(joinLong(makeLong(10 * N, i => 1)), 10 * N + ":0");
shouldBe(joinLong(makeLong(10 * N, i => i === 7 * N ? "s" : 1)), (10 * N - 1) + ":1");
shouldBe(joinLong(makeLong(10 * N, i => i >= 5 * N ? 1.5 : 1)), 12.5 * N + ":0");
shouldBe(joinLong(makeLong(10 * N, i => i === 10 * N - 1 ? {} : 1)), (10 * N - 1) + "[object Object]:0");

// 3. The array changes shape in the middle of a long loop.
function growWhileLooping(array) { let n = 0; for (let x of array) { n++; if (n === 5 * N) { array.push("tail"); array[10] = 0.5; } if (n === 6 * N) array.length = 7 * N; } return n; }
noInline(growWhileLooping);
for (let i = 0; i < 4; i++)
    shouldBe(growWhileLooping(makeLong(10 * N, i => i)), 7 * N);

// 4. break / return out of a loop that has been running optimized code for a while.
function findLong(array, what) { for (let x of array) { if (x === what) return x; } return -1; }
noInline(findLong);
{
    let array = makeLong(20 * N, i => i);
    shouldBe(findLong(array, 20 * N - 1), 20 * N - 1);
    shouldBe(findLong(array, 15 * N), 15 * N);
    shouldBe(findLong(array, -5), -1);
}

// 5. IteratorClose becomes observable after the loops above were compiled: the same functions still give the same answers.
let log = [];
ArrayIteratorPrototype.return = function () { log.push(Object.prototype.toString.call(this) + " " + JSON.stringify(originalNext.call(this))); return {}; };
shouldBe(sumLong(makeLong(32 * N, i => i & 3)), 48 * N);
shouldBe(findLong(makeLong(20 * N, i => i), 12), 12);
shouldBe(log.join("|"), '[object Array Iterator] {"value":13,"done":false}');
delete ArrayIteratorPrototype.return;

compiledReasonablyOften(sumLong);
compiledReasonablyOften(joinLong);
compiledReasonablyOften(growWhileLooping);
compiledReasonablyOften(findLong);
