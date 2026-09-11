// Regression test for the inline frame fast path of op_call_varargs when its argument is the spread
// butterfly of f(...x). Results must match the slow path across tiers, including bails for long or
// double-shaped butterflies, exceptions, stack overflow, and an OSR exit at the call.

function assert(b, msg) {
    if (!b)
        throw new Error("Bad assertion: " + msg);
}
noInline(assert);

function collect() { return Array.prototype.slice.call(arguments); }
noInline(collect);

function arrEq(x, y) {
    if (x.length !== y.length)
        return false;
    for (var i = 0; i < x.length; ++i) {
        var a = x[i], b = y[i];
        if (a !== b && !(a !== a && b !== b))
            return false;
    }
    return true;
}

function spread(arr) { return collect(...arr); }
noInline(spread);
function spreadRest(...rest) { return collect(...rest); }
noInline(spreadRest);
function spreadArguments() { return collect(...arguments); }
noInline(spreadArguments);
// Spreading a copy-on-write literal of non-numbers reuses the literal's butterfly.
function spreadContiguousLiteral() { var lit = ["a", "b", "c"]; return collect(...lit); }
noInline(spreadContiguousLiteral);
function spreadDoubleLiteral() { var lit = [1.5, 2.5]; return collect(...lit); }
noInline(spreadDoubleLiteral);
function spreadString(s) { return collect(...s); }
noInline(spreadString);

var empty = [];
var one = [10];
var many = [1, 2, 3, 4, 5];
var eight = [1, 2, 3, 4, 5, 6, 7, 8];
var nine = [1, 2, 3, 4, 5, 6, 7, 8, 9];
var holes = [, , 3];
var doubles = [0.5, NaN, 2.5];
var mixed = ["x", null, undefined, 2.5, true];

for (var i = 0; i < testLoopCount; ++i) {
    assert(arrEq(spread(empty), []), "empty");
    assert(arrEq(spread(one), [10]), "one");
    assert(arrEq(spread(many), [1, 2, 3, 4, 5]), "many");
    assert(arrEq(spread(eight), eight), "eight (longest inline)");
    assert(arrEq(spread(nine), nine), "nine (bail: too long)");
    assert(arrEq(spread(holes), [undefined, undefined, 3]), "holes");
    assert(arrEq(spread(doubles), [0.5, NaN, 2.5]), "doubles");
    assert(arrEq(spread(mixed), ["x", null, undefined, 2.5, true]), "mixed");
    assert(arrEq(spreadRest(1, "b", null), [1, "b", null]), "rest");
    assert(arrEq(spreadArguments(4, 5), [4, 5]), "arguments");
    assert(arrEq(spreadContiguousLiteral(), ["a", "b", "c"]), "contiguous literal");
    assert(arrEq(spreadDoubleLiteral(), [1.5, 2.5]), "double literal");
    assert(arrEq(spreadString("xyz"), ["x", "y", "z"]), "string");
}

// The callee is inlined with room for the argument counts seen while warming up, so a longer spread
// fails the count check and exits at the call.
function sumAll() {
    var sum = 0;
    for (var i = 0; i < arguments.length; ++i)
        sum += arguments[i];
    return sum;
}
function callInlined(arr) { return sumAll(...arr); }
noInline(callInlined);
for (var i = 0; i < testLoopCount; ++i)
    assert(callInlined([i, 1, 2]) === i + 3, "inlined warmup");
assert(callInlined([1, 2, 3, 4, 5, 6, 7]) === 28, "inlined exit on longer spread");

function thrower() { throw new Error("boom"); }
noInline(thrower);
function callThrower(arr) { return thrower(...arr); }
noInline(callThrower);
for (var i = 0; i < testLoopCount; ++i) {
    var threw = false;
    try { callThrower(many); } catch (e) { threw = e.message === "boom"; }
    assert(threw, "exception propagation");
}

// Stack overflow via a huge spread must throw RangeError, not corrupt the stack.
var huge = new Array(1000000).fill(1);
var gotRangeError = false;
try {
    spread(huge);
} catch (e) {
    gotRangeError = e instanceof RangeError;
}
assert(gotRangeError, "huge spread throws RangeError");

// Running out of stack while the fast path builds frames must bail to the slow path and throw.
function countArgs() { return arguments.length; }
noInline(countArgs);
var recursionThrewRangeError = false;
try {
    (function rec(n) { return countArgs(...many) + rec(n + 1); })(0);
} catch (e) {
    recursionThrewRangeError = e instanceof RangeError;
}
assert(recursionThrewRangeError, "deep recursion throws RangeError");
