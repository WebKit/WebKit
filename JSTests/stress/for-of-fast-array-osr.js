// A loop over an Array that runs without an iterator object changes tier in the middle: OSR entry from a long loop, OSR
// exit when a speculation fails half way, and code compiled only after IteratorClose became observable that has to take
// over frames opened before.

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

// 5. Destructuring in a hot function.
function swap(pair) { let [a, b] = pair; return [b, a]; }
function first3([a, b, c]) { return a + b + c; }
noInline(swap); noInline(first3);
{
    let p = [1, 2];
    for (let i = 0; i < 10 * N; i++)
        p = swap(p);
    shouldBe(p.join(), "1,2");
    let t = 0;
    for (let i = 0; i < 10 * N; i++)
        t += first3([1, 2, 3, 4]);
    shouldBe(t, 60 * N);
    shouldBe(String(first3([1, 2])), "NaN");
    shouldBe(first3("abc"), "abc");
    shouldBe(first3(new Set([1, 2, 3])), 6);
}

// 6. IteratorClose becomes observable in the middle of a long loop which then keeps running, enters optimized code
// compiled after the fact, and finally breaks: exactly one call, on an Array Iterator that is positioned right.
let log = [];
function lateReturn(array, installAt, breakAt) {
    let n = 0;
    for (let x of array) {
        if (n === installAt)
            ArrayIteratorPrototype.return = function () { log.push(Object.prototype.toString.call(this) + " " + JSON.stringify(originalNext.call(this))); return {}; };
        if (n === breakAt)
            break;
        n++;
    }
    return n;
}
noInline(lateReturn);
shouldBe(lateReturn(makeLong(40 * N, i => i), 10, 40 * N - 10), 40 * N - 10);
shouldBe(log.join("|"), '[object Array Iterator] {"value":' + (40 * N - 9) + ',"done":false}');
log = [];

// Now every close is observable. The same functions still give the same answers, and do not get stuck recompiling.
for (let i = 0; i < 3; i++)
    shouldBe(lateReturn(makeLong(20 * N, i => i), -1, 15 * N), 15 * N);
shouldBe(log.join("|"), new Array(3).fill('[object Array Iterator] {"value":' + (15 * N + 1) + ',"done":false}').join("|"));
log = [];
shouldBe(sumLong(makeLong(32 * N, i => i & 3)), 48 * N);
shouldBe(findLong(makeLong(20 * N, i => i), 12), 12);
shouldBe(log.join("|"), '[object Array Iterator] {"value":13,"done":false}');
log = [];
{
    let p = [1, 2, 3];
    for (let i = 0; i < 2 * N; i++)
        p = swap(p);
    shouldBe(p.join(), "1,2");
    shouldBe(log.length, 2 * N);
    log = [];
}
delete ArrayIteratorPrototype.return;
for (let i = 0; i < 2 * N; i++)
    swap([1, 2, 3]);
shouldBe(log.length, 0);

compiledReasonablyOften(sumLong);
compiledReasonablyOften(joinLong);
compiledReasonablyOften(growWhileLooping);
compiledReasonablyOften(findLong);
compiledReasonablyOften(swap);
compiledReasonablyOften(first3);
compiledReasonablyOften(lateReturn);
