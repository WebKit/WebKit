// Cyclic string conversions must follow the spec: they recurse until the stack is exhausted and
// then throw a RangeError. They must never silently substitute the empty string, and a conversion
// that happens to reuse a receiver already on the stack without actually being cyclic must produce
// its normal result.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function shouldThrowRangeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("didn't throw");
    if (!(error instanceof RangeError))
        throw new Error(`expected RangeError but got ${error}`);
}

// Array.prototype.join, Array.prototype.toString and Array.prototype.toLocaleString have no cycle
// detection in the specification, so a self-referential array overflows the stack.
shouldThrowRangeError(() => {
    let array = [];
    array[0] = array;
    return array.join();
});

shouldThrowRangeError(() => {
    let array = [];
    array[0] = array;
    return array.toString();
});

shouldThrowRangeError(() => {
    let array = [];
    array[0] = array;
    return `${array}`;
});

shouldThrowRangeError(() => {
    let array = [];
    array[0] = array;
    return array.toLocaleString();
});

// Indirect cycles overflow too.
shouldThrowRangeError(() => {
    let array = [1, "webkit"];
    array[2] = [3, 4, [5, 6, [array]]];
    return array.toString();
});

shouldThrowRangeError(() => {
    let array = ["a"];
    array.push({ toString() { return array.join("~"); } });
    return array.join("-");
});

// A cycle whose fan-out is larger than one must still terminate: the depth-first descent hits the
// stack limit and the RangeError propagates out instead of the join fanning out exponentially.
shouldThrowRangeError(() => {
    let array = [];
    array[0] = array;
    array[1] = array;
    return array.toString();
});

// Error.prototype.toString has no cycle detection either.
shouldThrowRangeError(() => {
    let error = new Error;
    error.name = error;
    error.message = error;
    return `${error}`;
});

shouldThrowRangeError(() => {
    let error = new Error;
    error.message = { toString() { return Error.prototype.toString.call(error); } };
    return `${error}`;
});

// Nor does RegExp.prototype.toString.
shouldThrowRangeError(() => {
    let regExp = /a/;
    Object.defineProperty(regExp, "source", { get() { return regExp; } });
    return `${regExp}`;
});

shouldThrowRangeError(() => {
    let regExp = /a/;
    Object.defineProperty(regExp, "flags", { get() { return RegExp.prototype.toString.call(regExp); } });
    return `${regExp}`;
});

// These conversions terminate on their own. Sharing a receiver with a conversion further up the
// stack is not a cycle, so each must return its ordinary result.
{
    let array = [];
    array[0] = { toString() { return Error.prototype.toString.call(array); } };
    shouldBe(array.join(), "Error");
}

{
    let array = [];
    array[0] = { toString() { return RegExp.prototype.toString.call(array); } };
    shouldBe(array.join(), "/undefined/undefined");
}

{
    let object = { length: 2, 0: "x", 1: "y" };
    object.name = { toString() { return Array.prototype.join.call(object, "-"); } };
    shouldBe(Error.prototype.toString.call(object), "x-y");
}

{
    let array = ["a"];
    array[1] = { toString() { return Array.prototype.toLocaleString.call({ length: 1, 0: "b" }); } };
    shouldBe(array.join("-"), "a-b");
}

// The receiver of an inner conversion being an ancestor's receiver is fine as long as the value
// graph is acyclic.
{
    let error = new Error;
    error.name = "E";
    error.message = { toString() { return Error.prototype.toString.call({ name: "inner", message: "m" }); } };
    shouldBe(`${error}`, "E: inner: m");
}

// A deep but acyclic nesting still converts, and a shared subtree is visited every time it occurs
// rather than being replaced by the empty string on the second visit.
{
    let shared = [1, 2];
    shouldBe([shared, shared].toString(), "1,2,1,2");
    shouldBe([shared, shared].toLocaleString(), "1,2,1,2");
    shouldBe([shared, shared].join("|"), "1,2|1,2");
}

// Recovering after a stack overflow must leave no state behind that suppresses later conversions.
{
    let array = [];
    array[0] = array;
    for (let i = 0; i < 2; ++i)
        shouldThrowRangeError(() => array.toString());
    shouldBe([1, 2].toString(), "1,2");

    let error = new Error;
    error.name = error;
    shouldThrowRangeError(() => `${error}`);
    shouldBe(`${new Error("m")}`, "Error: m");
}

// The optimizing tiers reach the array conversions through their own paths, so warm them up on an
// acyclic array and then check that a cyclic one still overflows there and that the overflow leaves
// the warmed-up conversions intact.
{
    const convert = a => `${a}`;
    const join = a => a.join("-");
    const toLocale = a => a.toLocaleString();
    noInline(convert);
    noInline(join);
    noInline(toLocale);

    let acyclic = [1, 2, 3];
    let cyclic = [];
    cyclic[0] = cyclic;

    for (let i = 0; i < 20000; ++i) {
        shouldBe(convert(acyclic), "1,2,3");
        shouldBe(join(acyclic), "1-2-3");
        shouldBe(toLocale(acyclic), "1,2,3");
    }

    for (const f of [convert, join, toLocale]) {
        shouldThrowRangeError(() => f(cyclic));
        shouldThrowRangeError(() => f(cyclic));
    }

    shouldBe(convert(acyclic), "1,2,3");
    shouldBe(join(acyclic), "1-2-3");
    shouldBe(toLocale(acyclic), "1,2,3");
}
