function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + ", expected " + String(expected));
}

function shouldThrow(func, errorType) {
    let error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error("expected " + errorType.name + ", got " + String(error));
}

// An array argument with @@isConcatSpreadable = false is appended whole.
for (let i = 0; i < testLoopCount; i++) {
    let ns = [7, 8];
    ns[Symbol.isConcatSpreadable] = false;
    let r = [1].concat([2], ns);
    shouldBe(r.length, 3);
    shouldBe(r[2], ns);
}

// A plain object with @@isConcatSpreadable = true is spread.
for (let i = 0; i < testLoopCount; i++) {
    let fake = { length: 2, 0: "p", 1: "q" };
    fake[Symbol.isConcatSpreadable] = true;
    shouldBe([1].concat(fake, [2]).join(","), "1,p,q,2");
}

// @@isConcatSpreadable = false on Array.prototype affects the receiver and all array arguments.
{
    Array.prototype[Symbol.isConcatSpreadable] = false;
    let r = [1].concat([2], [3]);
    shouldBe(r.length, 3);
    shouldBe(r[0][0], 1);
    shouldBe(r[1][0], 2);
    shouldBe(r[2][0], 3);
    delete Array.prototype[Symbol.isConcatSpreadable];
}

// A @@isConcatSpreadable getter runs left to right and its side effects on later
// arguments must be observed.
for (let i = 0; i < testLoopCount; i++) {
    let order = [];
    let later = [1, 2, 3];
    let tricky = {
        length: 1,
        0: "t",
        get [Symbol.isConcatSpreadable]() {
            order.push("tricky");
            later.length = 1;
            return true;
        }
    };
    let r = [0].concat(tricky, later);
    shouldBe(order.join(","), "tricky");
    shouldBe(r.join(","), "0,t,1");
}

// A huge fake spreadable must throw as soon as the accumulated length exceeds the
// maximum. The receiver already contributes one element, so appending length 2^53 - 1
// overflows immediately, before the element-copying loop starts.
shouldThrow(() => {
    let huge = { length: 2 ** 53 - 1 };
    huge[Symbol.isConcatSpreadable] = true;
    [1].concat(huge, [2]);
}, TypeError);
