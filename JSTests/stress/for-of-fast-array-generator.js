// Generators and async functions save their frame when they suspend. A for-of over an Array that is running without an
// iterator object is saved as it is (a marker, the Array and the index) and must pick up where it left off, in whatever
// tier the function resumes in, including after IteratorClose became observable while it was suspended.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

const ArrayIteratorPrototype = Object.getPrototypeOf([][Symbol.iterator]());
const originalNext = ArrayIteratorPrototype.next;
let log = [];

function* each(array) {
    for (let x of array)
        yield x;
    return "end";
}

function* eachWithFinally(array) {
    try {
        for (let x of array) {
            let sent = yield x;
            if (sent === "stop")
                break;
        }
    } finally {
        log.push("finally");
    }
    return "end";
}

function* nestedLoops(array) {
    for (let x of array) {
        for (let y of array) {
            yield x * 10 + y;
            if (y === 2)
                break;
        }
    }
}

function* destructureAcrossYield(array) {
    let [a, b = yield "need b", c = yield "need c"] = array;
    return [a, b, c].join();
}

async function eachAsync(array, out) {
    for (let x of array) {
        await null;
        out.push(x);
        if (x === 3)
            break;
    }
    return out.join();
}

function drain(generator) { let values = []; for (;;) { let r = generator.next(); if (r.done) { values.push("done:" + r.value); break; } values.push(r.value); } return values.join(); }

// Warm up: many generators suspended and resumed, in all tiers.
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(drain(each([1, 2, 3])), "1,2,3,done:end");
    shouldBe(drain(nestedLoops([1, 2, 3])), "11,12,21,22,31,32,done:undefined");
    let g = eachWithFinally([1, 2, 3]);
    g.next(); g.next();
    shouldBe(JSON.stringify(g.next("stop")), '{"value":"end","done":true}');
    let d = destructureAcrossYield([1]);
    shouldBe(d.next().value, "need b");
    shouldBe(d.next("B").value, "need c");
    shouldBe(d.next("C").value, "1,B,C");
    let out = [];
    let result;
    eachAsync([1, 2, 3, 4], out).then(v => result = v);
    drainMicrotasks();
    shouldBe(result, "1,2,3");
}
shouldBe(log.length, testLoopCount);

// The array changes while the generator is suspended.
{
    let array = [1, 2, 3];
    let g = each(array);
    shouldBe(g.next().value, 1);
    array.push(4);
    array[1] = "two";
    shouldBe(g.next().value, "two");
    array.length = 2;
    shouldBe(JSON.stringify(g.next()), '{"value":"end","done":true}');
    array.push(9);
    shouldBe(JSON.stringify(g.next()), '{"done":true}');
}

// Suspended in the middle of their loops, in the state with no iterator object.
let array = [1, 2, 3, 4];
let suspendedEach = each(array);
shouldBe(suspendedEach.next().value, 1);
let suspendedFinally = eachWithFinally(array);
shouldBe(suspendedFinally.next().value, 1);
shouldBe(suspendedFinally.next().value, 2);
let suspendedReturn = eachWithFinally(array);
shouldBe(suspendedReturn.next().value, 1);
let suspendedNested = nestedLoops(array);
shouldBe(suspendedNested.next().value, 11);
let suspendedDestructure = destructureAcrossYield([1]);
shouldBe(suspendedDestructure.next().value, "need b");
let asyncOut = [];
let asyncResult;
let release;
async function suspendedAsync(array) {
    for (let x of array) {
        asyncOut.push(x);
        if (x === 2)
            await new Promise(resolve => release = resolve);
        if (x === 3)
            break;
    }
    return asyncOut.join();
}
suspendedAsync(array).then(v => asyncResult = v);
drainMicrotasks();
shouldBe(asyncOut.join(), "1,2");

// IteratorClose becomes observable while they sleep.
log = [];
ArrayIteratorPrototype.return = function () {
    log.push("return " + Object.prototype.toString.call(this) + " " + JSON.stringify(originalNext.call(this)));
    return {};
};

// Make sure the generator functions are compiled again, now with an observable protocol, before the old frames resume.
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(drain(each([1, 2])), "1,2,done:end");
    drain(nestedLoops([1, 2]));
}
shouldBe(log.length, testLoopCount * 2); // nestedLoops breaks out of its inner loop twice per run.
log = [];

// Runs to the end: no close.
shouldBe(drain(suspendedEach), "2,3,4,done:end");
shouldBe(log.join("|"), "");

// break after resuming: close on an iterator positioned after 2.
shouldBe(JSON.stringify(suspendedFinally.next("stop")), '{"value":"end","done":true}');
shouldBe(log.join("|"), 'return [object Array Iterator] {"value":3,"done":false}|finally');
log = [];

// generator.return() while suspended inside the loop: close on an iterator positioned after 1.
shouldBe(JSON.stringify(suspendedReturn.return("bye")), '{"value":"bye","done":true}');
shouldBe(log.join("|"), 'return [object Array Iterator] {"value":2,"done":false}|finally');
log = [];

// Inner loop breaks at y === 2, outer keeps going.
shouldBe(suspendedNested.next().value, 12);
shouldBe(suspendedNested.next().value, 21);
shouldBe(log.join("|"), 'return [object Array Iterator] {"value":3,"done":false}');
suspendedNested.return();
shouldBe(log.length, 3); // inner loop, then outer loop
log = [];

// Destructuring that was suspended inside a default value: the iterator is already exhausted, nothing to close.
shouldBe(suspendedDestructure.next("B").value, "need c");
shouldBe(suspendedDestructure.next("C").value, "1,B,C");
shouldBe(log.join("|"), "");

release();
drainMicrotasks();
shouldBe(asyncResult, "1,2,3");
shouldBe(log.join("|"), 'return [object Array Iterator] {"value":4,"done":false}');

delete ArrayIteratorPrototype.return;
