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

function drain(generator) { let values = []; for (;;) { let r = generator.next(); if (r.done) { values.push("done:" + r.value); break; } values.push(r.value); } return values.join(); }

// Warm up: many generators suspended and resumed, in all tiers.
for (let i = 0; i < testLoopCount; i++)
    shouldBe(drain(each([1, 2, 3])), "1,2,3,done:end");

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

// IteratorClose becomes observable while they sleep.
log = [];
ArrayIteratorPrototype.return = function () {
    log.push("return " + Object.prototype.toString.call(this) + " " + JSON.stringify(originalNext.call(this)));
    return {};
};

// Make sure the generator functions are compiled again, now with an observable protocol, before the old frames resume.
for (let i = 0; i < testLoopCount; i++)
    shouldBe(drain(each([1, 2])), "1,2,done:end");
shouldBe(log.length, 0);

// Runs to the end: no close.
shouldBe(drain(suspendedEach), "2,3,4,done:end");
shouldBe(log.join("|"), "");

delete ArrayIteratorPrototype.return;
