// Exercises the fused mixed literal+spread call opcode (op_call_varargs_with_spread) for
// f(a, ...b, c)-style calls; results must match the slow new_array_with_spread path across all tiers.

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

function prefix1(a, arr) { return collect(a, ...arr); }
noInline(prefix1);
function prefix2(a, b, arr) { return collect(a, b, ...arr); }
noInline(prefix2);
function midSpread(a, arr, z) { return collect(a, ...arr, z); }
noInline(midSpread);
function twoSpread(arr1, arr2) { return collect(...arr1, ...arr2); }
noInline(twoSpread);
function spreadFirst(arr, z) { return collect(...arr, z); }
noInline(spreadFirst);

var empty = [];
var one = [10];
var many = [1, 2, 3, 4, 5];
var holes = [, , 3];
var mixed = ["x", null, undefined, 2.5, true];

for (var i = 0; i < testLoopCount; ++i) {
    assert(arrEq(prefix1(0, empty), [0]), "prefix1 empty");
    assert(arrEq(prefix1(7, one), [7, 10]), "prefix1 one");
    assert(arrEq(prefix1(7, many), [7, 1, 2, 3, 4, 5]), "prefix1 many");
    assert(arrEq(prefix1(9, holes), [9, undefined, undefined, 3]), "prefix1 holes");
    assert(arrEq(prefix2("a", "b", empty), ["a", "b"]), "prefix2 empty");
    assert(arrEq(prefix2("a", "b", many), ["a", "b", 1, 2, 3, 4, 5]), "prefix2 many");
    assert(arrEq(prefix2(1, 2, mixed), [1, 2, "x", null, undefined, 2.5, true]), "prefix2 mixed");
    assert(arrEq(midSpread("a", many, "z"), ["a", 1, 2, 3, 4, 5, "z"]), "midSpread many");
    assert(arrEq(midSpread("a", empty, "z"), ["a", "z"]), "midSpread empty");
    assert(arrEq(twoSpread(one, many), [10, 1, 2, 3, 4, 5]), "twoSpread");
    assert(arrEq(twoSpread(empty, empty), []), "twoSpread empty");
    assert(arrEq(spreadFirst(many, 99), [1, 2, 3, 4, 5, 99]), "spreadFirst");
}

var obj = {
    tag: "T",
    m() { return this.tag + "|" + Array.prototype.join.call(arguments, ","); }
};
function callMethod(a, arr) { return obj.m(a, ...arr); }
noInline(callMethod);
for (var i = 0; i < testLoopCount; ++i)
    assert(callMethod(1, [2, 3]) === "T|1,2,3", "method this + prefix spread");

// OSR-exit path: force a speculation failure while interleaved arguments are live.
var sideEffected = false;
function observe(x) { if (typeof x !== "number") sideEffected = true; return x; }
noInline(observe);
function fooExit(a, arr) {
    observe(a);
    var r = collect(a, ...arr, a);
    return r;
}
noInline(fooExit);
for (var i = 0; i < testLoopCount; ++i) {
    var r = fooExit(i, [i + 1, i + 2]);
    assert(arrEq(r, [i, i + 1, i + 2, i]), "fooExit warmup");
}
var marker = {};
var rExit = fooExit(marker, [20, 30]);
assert(rExit.length === 4 && rExit[0] === marker && rExit[1] === 20 && rExit[2] === 30 && rExit[3] === marker, "fooExit exit result");

// Exceptions from the callee must propagate.
function thrower() { throw new Error("boom"); }
noInline(thrower);
function callThrower(a, arr) { return thrower(a, ...arr); }
noInline(callThrower);
for (var i = 0; i < testLoopCount; ++i) {
    var threw = false;
    try { callThrower(1, many); } catch (e) { threw = e.message === "boom"; }
    assert(threw, "exception propagation");
}

// A huge spread must throw RangeError, not corrupt the stack.
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
