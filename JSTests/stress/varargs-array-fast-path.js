// Regression test for the small-array varargs fast path of op_call_varargs (spread calls f(a, ...arr)).
// Fast-path results must match the slow path across tiers, incl. holes, exceptions, stack overflow, OSR exit.

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

function call1(a, arr) { return collect(a, ...arr); }
noInline(call1);
function call2(a, b, arr) { return collect(a, b, ...arr); }
noInline(call2);

var empty = [];
var one = [10];
var many = [1, 2, 3, 4, 5];
var holes = [, , 3];
var mixed = ["x", null, undefined, 2.5, true];

for (var i = 0; i < testLoopCount; ++i) {
    assert(arrEq(call1(0, empty), [0]), "call1 empty");
    assert(arrEq(call1(7, one), [7, 10]), "call1 one");
    assert(arrEq(call1(7, many), [7, 1, 2, 3, 4, 5]), "call1 many");
    assert(arrEq(call1(9, holes), [9, undefined, undefined, 3]), "call1 holes");
    assert(arrEq(call2("a", "b", empty), ["a", "b"]), "call2 empty");
    assert(arrEq(call2("a", "b", many), ["a", "b", 1, 2, 3, 4, 5]), "call2 many");
    assert(arrEq(call2(1, 2, mixed), [1, 2, "x", null, undefined, 2.5, true]), "call2 mixed");
}

// OSR exit: force a speculation failure while the merged argument array is live (non-number "a").
var sideEffected = false;
function observe(x) { if (typeof x !== "number") sideEffected = true; return x; }
noInline(observe);
function effect() { sideEffected = true; }
noInline(effect);

function fooExit(a, arr) {
    observe(a);
    var r = collect(a, ...arr);
    if (sideEffected)
        effect();
    return r;
}
noInline(fooExit);

for (var i = 0; i < testLoopCount; ++i) {
    var r = fooExit(i, [i + 1, i + 2]);
    assert(arrEq(r, [i, i + 1, i + 2]), "fooExit warmup");
}
var obj = {};
var rExit = fooExit(obj, [20, 30]);
assert(rExit.length === 3 && rExit[0] === obj && rExit[1] === 20 && rExit[2] === 30, "fooExit exit result");
assert(sideEffected, "fooExit side effect ran");

function thrower() { throw new Error("boom"); }
noInline(thrower);
function callThrower(a, arr) { return thrower(a, ...arr); }
noInline(callThrower);
for (var i = 0; i < testLoopCount; ++i) {
    var threw = false;
    try { callThrower(1, many); } catch (e) { threw = e.message === "boom"; }
    assert(threw, "exception propagation");
}

// Stack overflow via a huge spread must throw RangeError, not corrupt the stack.
var huge = new Array(1000000).fill(1);
var gotRangeError = false;
try {
    (function overflow(a, arr) { return collect(a, ...arr); })(0, huge);
} catch (e) {
    gotRangeError = e instanceof RangeError;
}
assert(gotRangeError, "huge spread throws RangeError");

// Running out of stack through recursion rather than argument count must unwind just as cleanly.
function countArgs() { return arguments.length; }
noInline(countArgs);
var recursionThrewRangeError = false;
try {
    (function rec(n) { return countArgs(n, ...many) + rec(n + 1); })(0);
} catch (e) {
    recursionThrewRangeError = e instanceof RangeError;
}
assert(recursionThrewRangeError, "deep recursion throws RangeError");
