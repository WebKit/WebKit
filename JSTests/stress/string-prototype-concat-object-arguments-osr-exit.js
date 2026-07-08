function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function shouldThrow(func, errorType) {
    let threw = false;
    try {
        func();
    } catch (error) {
        threw = true;
        if (!(error instanceof errorType))
            throw new Error(`bad error: ${error}`);
    }
    if (!threw)
        throw new Error("should throw");
}

function makeCountingObject(result) {
    const counters = { toString: 0, valueOf: 0 };
    const object = {
        toString() {
            ++counters.toString;
            return result;
        },
        valueOf() {
            ++counters.valueOf;
            throw new Error("valueOf should not be called");
        },
    };
    return { object, counters };
}

// Warmed up with string arguments, then an object argument causes OSR exit.
// toString() must be observed exactly once per call and valueOf() never.
function concatAfterStringWarmUp(str, x) {
    return str.concat(x, "!");
}
noInline(concatAfterStringWarmUp);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(concatAfterStringWarmUp("foo", "bar"), "foobar!");
{
    const { object, counters } = makeCountingObject("object");
    for (let i = 0; i < 100; ++i)
        shouldBe(concatAfterStringWarmUp("foo", object), "fooobject!");
    shouldBe(counters.toString, 100);
    shouldBe(counters.valueOf, 0);
}

// Warmed up with int32 arguments (non-cell speculation), then an object argument.
function concatAfterInt32WarmUp(str, x) {
    return str.concat(x, "px");
}
noInline(concatAfterInt32WarmUp);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(concatAfterInt32WarmUp("w:", 42), "w:42px");
{
    const { object, counters } = makeCountingObject("object");
    for (let i = 0; i < 100; ++i)
        shouldBe(concatAfterInt32WarmUp("w:", object), "w:objectpx");
    shouldBe(counters.toString, 100);
    shouldBe(counters.valueOf, 0);
}

// Warmed up with string-or-undefined/null arguments, then an object argument.
function concatAfterStringOrOtherWarmUp(str, x) {
    return str.concat(x);
}
noInline(concatAfterStringOrOtherWarmUp);

for (let i = 0; i < testLoopCount; ++i) {
    if (i & 1)
        shouldBe(concatAfterStringOrOtherWarmUp("v:", undefined), "v:undefined");
    else
        shouldBe(concatAfterStringOrOtherWarmUp("v:", null), "v:null");
}
{
    const { object, counters } = makeCountingObject("object");
    for (let i = 0; i < 100; ++i)
        shouldBe(concatAfterStringOrOtherWarmUp("v:", object), "v:object");
    shouldBe(counters.toString, 100);
    shouldBe(counters.valueOf, 0);
}

// Warmed up with a string receiver, then an object receiver causes OSR exit.
function concatWithReceiver(receiver, x) {
    return String.prototype.concat.call(receiver, x);
}
noInline(concatWithReceiver);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(concatWithReceiver("head", "tail"), "headtail");
{
    const { object, counters } = makeCountingObject("object");
    for (let i = 0; i < 100; ++i)
        shouldBe(concatWithReceiver(object, "tail"), "objecttail");
    shouldBe(counters.toString, 100);
    shouldBe(counters.valueOf, 0);
}

// More than maxStrCatArguments arguments so that chained StrCat nodes are
// emitted. Objects in the middle of the argument list must observe toString()
// exactly once per call and in argument order, even though the OSR exit
// happens after some of the chained StrCat nodes.
function concatManyArguments(receiver, a, b, c, d, e) {
    return String.prototype.concat.call(receiver, a, b, c, d, e);
}
noInline(concatManyArguments);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(concatManyArguments("A", "B", "C", "D", "E", "F"), "ABCDEF");
{
    const order = [];
    function makeOrderedObject(name) {
        return {
            toString() {
                order.push(name);
                return name;
            },
        };
    }
    const receiver = makeOrderedObject("R");
    const b = makeOrderedObject("b");
    const e = makeOrderedObject("e");
    for (let i = 0; i < 100; ++i) {
        order.length = 0;
        shouldBe(concatManyArguments(receiver, "1", b, "3", e, "5"), "R1b3e5");
        shouldBe(order.join(","), "R,b,e");
    }
}

// An object whose Symbol.toPrimitive is used for the conversion.
function concatToPrimitive(str, x) {
    return str.concat(x);
}
noInline(concatToPrimitive);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(concatToPrimitive("foo", "bar"), "foobar");
{
    let toPrimitiveCalls = 0;
    const object = {
        [Symbol.toPrimitive](hint) {
            ++toPrimitiveCalls;
            shouldBe(hint, "string");
            return "object";
        },
        toString() {
            throw new Error("toString should not be called");
        },
        valueOf() {
            throw new Error("valueOf should not be called");
        },
    };
    for (let i = 0; i < 100; ++i)
        shouldBe(concatToPrimitive("foo", object), "fooobject");
    shouldBe(toPrimitiveCalls, 100);
}

// A symbol argument must throw a TypeError even after warming up with strings.
function concatSymbol(str, x) {
    return str.concat(x);
}
noInline(concatSymbol);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(concatSymbol("foo", "bar"), "foobar");
for (let i = 0; i < 100; ++i)
    shouldThrow(() => concatSymbol("foo", Symbol("sym")), TypeError);
