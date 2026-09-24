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

function* nestedLoops(array) {
    for (let x of array) {
        for (let y of array) {
            yield x * 10 + y;
            if (y === 2)
                break;
        }
    }
}

function drain(generator) { let values = []; for (;;) { let r = generator.next(); if (r.done) { values.push("done:" + r.value); break; } values.push(r.value); } return values.join(); }

// Warm up: many generators suspended and resumed, in all tiers.
for (let i = 0; i < testLoopCount; i++)
    shouldBe(drain(nestedLoops([1, 2, 3])), "11,12,21,22,31,32,done:undefined");

// Suspended in the middle of their loops, in the state with no iterator object.
let array = [1, 2, 3, 4];
let suspendedNested = nestedLoops(array);
shouldBe(suspendedNested.next().value, 11);

// IteratorClose becomes observable while they sleep.
log = [];
ArrayIteratorPrototype.return = function () {
    log.push("return " + Object.prototype.toString.call(this) + " " + JSON.stringify(originalNext.call(this)));
    return {};
};

// Make sure the generator functions are compiled again, now with an observable protocol, before the old frames resume.
for (let i = 0; i < testLoopCount; i++)
    drain(nestedLoops([1, 2]));
shouldBe(log.length, testLoopCount * 2); // nestedLoops breaks out of its inner loop twice per run.
log = [];

// Inner loop breaks at y === 2, outer keeps going.
shouldBe(suspendedNested.next().value, 12);
shouldBe(suspendedNested.next().value, 21);
shouldBe(log.join("|"), 'return [object Array Iterator] {"value":3,"done":false}');
suspendedNested.return();
shouldBe(log.length, 3); // inner loop, then outer loop

delete ArrayIteratorPrototype.return;
