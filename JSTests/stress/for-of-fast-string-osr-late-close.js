// A loop over a String that runs without an iterator object meets an observable IteratorClose half way: code compiled only
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

const StringIteratorPrototype = Object.getPrototypeOf(""[Symbol.iterator]());
const originalNext = StringIteratorPrototype.next;

const N = testLoopCount;

function find(iterable, wanted) { let n = 0; for (let x of iterable) { if (x === wanted) break; n++; } return n; }
function swap(pair) { let [a, b] = pair; return b + a; }
noInline(find); noInline(swap);
for (let i = 0; i < N; i++) {
    shouldBe(find("abcz", "z"), 3);
    shouldBe(find([1, 2, 3, 4], 3), 2);
    shouldBe(swap("ab"), "ba");
}

// IteratorClose becomes observable in the middle of a long loop which then keeps running, enters optimized code compiled
// after the fact, and finally breaks: exactly one call, on a String Iterator that is positioned right.
let log = [];
function lateReturn(string, installAt, breakAt) {
    let n = 0;
    for (let x of string) {
        if (n === installAt)
            StringIteratorPrototype.return = function () { log.push(Object.prototype.toString.call(this) + " " + JSON.stringify(originalNext.call(this))); return {}; };
        if (n === breakAt)
            break;
        n++;
    }
    return n;
}
noInline(lateReturn);
shouldBe(lateReturn("x".repeat(30 * N - 9) + "yz", 10, 30 * N - 10), 30 * N - 10);
shouldBe(log.join("|"), '[object String Iterator] {"value":"y","done":false}');
log = [];

// Now every close is observable. The same functions still give the same answers, and do not get stuck recompiling.
for (let i = 0; i < 3; i++) {
    shouldBe(find("x".repeat(10 * N) + "z", "z"), 10 * N);
    shouldBe(log.join("|"), '[object String Iterator] {"done":true}');
    log = [];
}
for (let i = 0; i < N; i++)
    shouldBe(find([1, 2, 3, 4], 3), 2);
shouldBe(log.length, 0);
for (let i = 0; i < N; i++) {
    shouldBe(swap("ab"), "ba");
    shouldBe(swap("abc"), "ba");
}
shouldBe(log.length, 2 * N);
shouldBe(log[0], '[object String Iterator] {"done":true}');
shouldBe(log[1], '[object String Iterator] {"value":"c","done":false}');
compiledReasonablyOften(find);
compiledReasonablyOften(swap);
compiledReasonablyOften(lateReturn);
