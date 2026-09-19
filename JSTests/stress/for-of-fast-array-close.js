// for-of and array destructuring over an Array may run without an Array Iterator object. Whenever IteratorClose becomes
// observable (a "return" property shows up on the iterator's prototype chain), the object the loop never made has to be there,
// be an Array Iterator, belong to the right Array and be positioned after the last element that was visited.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

const ArrayIteratorPrototype = Object.getPrototypeOf([][Symbol.iterator]());
const IteratorPrototype = Object.getPrototypeOf(ArrayIteratorPrototype);
const originalNext = ArrayIteratorPrototype.next;

let log = [];
let seenIterators = [];
function installReturn(where) {
    where.return = function () {
        // The index is observable through a next() on the iterator that was handed to us.
        let step = originalNext.call(this);
        log.push("return " + Object.prototype.toString.call(this) + " " + (Object.getPrototypeOf(this) === ArrayIteratorPrototype) + " " + JSON.stringify(step));
        seenIterators.push(this);
        return {};
    };
}
function removeReturn(where) { delete where.return; }

// Every way out of a loop. `hook` runs in the loop body at element `at`.
function breakOut(array, at, hook) { let seen = []; for (let x of array) { seen.push(x); if (x === at) { hook(); break; } } return seen.join(); }
function returnOut(array, at, hook) { let seen = []; for (let x of array) { seen.push(x); if (x === at) { hook(); return seen.join(); } } return seen.join(); }
function throwOut(array, at, hook) { let seen = []; try { for (let x of array) { seen.push(x); if (x === at) { hook(); throw new Error("out"); } } } catch (e) { seen.push(e.message); } return seen.join(); }
function continueOuter(array, at, hook) { let seen = []; outer: for (let i = 0; i < 2; i++) { for (let x of array) { seen.push(x); if (x === at) { hook(); continue outer; } } } return seen.join(); }
function breakOuter(array, at, hook) { let seen = []; outer: for (let i = 0; i < 2; i++) { for (let x of array) { seen.push(x); if (x === at) { hook(); break outer; } } } return seen.join(); }
function finallyOut(array, at, hook) { let seen = []; try { for (let x of array) { seen.push(x); if (x === at) { hook(); return seen.join(); } } } finally { seen.push("finally"); log.push("finally " + seen.join()); } return seen.join(); }
function runToEnd(array, at, hook) { let seen = []; for (let x of array) { seen.push(x); if (x === at) hook(); } return seen.join(); }
function nested(array, at, hook) { let seen = []; for (let x of array) { for (let y of array) { seen.push(x * 10 + y); if (y === at) { hook(); break; } } if (x === at) break; } return seen.join(); }
function destructure2(array, at, hook) { hook(); let [a, b] = array; return a + "," + b; }
function destructureDefault(array, at, hook) { let [a, b = (hook(), "d")] = array; return a + "," + b; }
function destructureEmpty(array, at, hook) { hook(); let [] = array; return ""; }
function destructureRest(array, at, hook) { hook(); let [a, ...r] = array; return a + "," + r.join(); }

const noHook = () => { };
function warm(f, array, at) { for (let i = 0; i < testLoopCount; i++) f(array(), at, noHook); }

let scenarios = [
    [breakOut, 2, "1,2", 'return [object Array Iterator] true {"value":3,"done":false}'],
    [returnOut, 3, "1,2,3", 'return [object Array Iterator] true {"value":4,"done":false}'],
    [throwOut, 1, "1,out", 'return [object Array Iterator] true {"value":2,"done":false}'],
    [continueOuter, 4, "1,2,3,4,1,2,3,4", 'return [object Array Iterator] true {"done":true}|return [object Array Iterator] true {"done":true}'],
    [breakOuter, 2, "1,2", 'return [object Array Iterator] true {"value":3,"done":false}'],
    [finallyOut, 2, "1,2", 'return [object Array Iterator] true {"value":3,"done":false}|finally 1,2,finally'],
    [runToEnd, 2, "1,2,3,4", ""],
    [nested, 2, "11,12,21,22", 'return [object Array Iterator] true {"value":3,"done":false}|return [object Array Iterator] true {"value":3,"done":false}|return [object Array Iterator] true {"value":3,"done":false}'],
    [destructure2, 0, "1,2", 'return [object Array Iterator] true {"value":3,"done":false}'],
    [destructureDefault, 0, "1,2", 'return [object Array Iterator] true {"value":3,"done":false}'],
    [destructureEmpty, 0, "", 'return [object Array Iterator] true {"value":1,"done":false}'],
    [destructureRest, 0, "1,2,3,4", ""],
];

// 1. Nothing is observable while no "return" exists; this also warms every function up in the state where there is no iterator object.
for (let [f, at, expected] of scenarios) {
    warm(f, () => [1, 2, 3, 4], at);
    log = [];
    shouldBe(f([1, 2, 3, 4], at, noHook), expected, f.name);
    shouldBe(log.filter(s => s.startsWith("return")).length, 0, f.name + " quiet");
}

// 2. "return" installed from inside the loop body on each of the three prototypes in turn. The close that follows must call it
// on a real Array Iterator at the right position. Only the very first case still opens its loop without an iterator object:
// from then on this realm has seen an observable IteratorClose (watchpoints do not come back) and the functions compiled for
// the no-object state have to cope with loops that do have one. for-of-fast-array-close-fresh-realm.js gives every case a
// realm of its own.
let holders = [ArrayIteratorPrototype, IteratorPrototype, Object.prototype];
let round = 0;
for (let holder of holders) {
    for (let [f, at, expected, expectedLog] of scenarios) {
        log = [];
        seenIterators = [];
        let array = [1, 2, 3, 4];
        let result = f(array, at, () => installReturn(holder));
        removeReturn(holder);
        // destructureDefault's second element exists, so its hook never runs.
        if (f === destructureDefault)
            expectedLog = round ? expectedLog : "";
        // With the hook run before the loop is opened (destructure2 / destructureEmpty / destructureRest), opening sees an observable protocol: same answer.
        if (f === finallyOut)
            shouldBe(log.join("|"), expectedLog, f.name + " log (" + round + ")");
        else
            shouldBe(log.filter(s => s.startsWith("return")).join("|"), f === destructureDefault ? "" : expectedLog, f.name + " log (" + round + ")");
        shouldBe(result, expected, f.name + " result (" + round + ")");
        for (let iterator of seenIterators) {
            shouldBe(typeof iterator, "object");
            shouldBe(Object.getPrototypeOf(iterator), ArrayIteratorPrototype);
        }
        // Each close saw its own object.
        shouldBe(new Set(seenIterators).size, seenIterators.length, f.name + " distinct iterators");
    }
    round++;
}

// 3. From now on the protocol is observable for good (watchpoints do not come back). With "return" installed all the time, every
// close calls it; functions already compiled for the no-object state keep giving the same answers.
let closes = 0;
ArrayIteratorPrototype.return = function () { closes++; return {}; };
for (let [f, at, expected, expectedLog] of scenarios) {
    if (f === destructureDefault)
        continue;
    let expectedCloses = expectedLog.split("|").filter(s => s.startsWith("return")).length;
    for (let i = 0; i < testLoopCount; i++) {
        closes = 0;
        log = [];
        shouldBe(f([1, 2, 3, 4], at, noHook), expected, f.name + " after");
        shouldBe(closes, expectedCloses, f.name + " closes after");
    }
}
installReturn(ArrayIteratorPrototype);
for (let [f, at, expected, expectedLog] of scenarios) {
    if (f === destructureDefault)
        continue;
    log = [];
    shouldBe(f([1, 2, 3, 4], at, noHook), expected, f.name + " after");
    if (f === finallyOut)
        shouldBe(log.join("|"), expectedLog, f.name + " log after");
    else
        shouldBe(log.filter(s => s.startsWith("return")).join("|"), expectedLog, f.name + " log after");
}
removeReturn(ArrayIteratorPrototype);

// 4. The spec fixes the next method when the loop is opened: replacing it mid-loop changes nothing for a loop in progress.
// By now every loop in this realm has an iterator object; for-of-fast-array-close-fresh-realm.js has the loop without one.
{
    let calls = 0;
    function patchedNextMidLoop(array) {
        let seen = [];
        for (let x of array) {
            seen.push(x);
            if (x === 2)
                ArrayIteratorPrototype.next = function () { calls++; return originalNext.call(this); };
        }
        return seen.join();
    }
    shouldBe(patchedNextMidLoop([1, 2, 3, 4]), "1,2,3,4");
    shouldBe(calls, 0);
    shouldBe(patchedNextMidLoop([1, 2, 3, 4]), "1,2,3,4");
    shouldBe(calls, 5);
    ArrayIteratorPrototype.next = originalNext;
}
