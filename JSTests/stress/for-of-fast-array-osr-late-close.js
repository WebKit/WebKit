// A loop over an Array that runs without an iterator object meets an observable IteratorClose half way: code compiled only
// after that has to take over frames opened before.

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

// 1. Destructuring in a hot function.
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

// 2. IteratorClose becomes observable in the middle of a long loop which then keeps running, enters optimized code
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

compiledReasonablyOften(swap);
compiledReasonablyOften(first3);
compiledReasonablyOften(lateReturn);
