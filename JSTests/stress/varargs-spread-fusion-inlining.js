// Regression test for DFG/FTL inlining of the fused literal+spread call opcode
// (op_call_varargs_with_spread) expanding interleaved operands into the inlined frame.

function assert(b, msg) {
    if (!b)
        throw new Error("Bad assertion: " + msg);
}
noInline(assert);

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
noInline(arrEq);

// Small, inlinable callees (NOT noInline) so op_call_varargs_with_spread inlines them.
function tiny(a, b) { return (a | 0) + (b | 0) * 2; }
function sum5(a, b, c, d, e) { return (a | 0) + (b | 0) + (c | 0) + (d | 0) + (e | 0); }
function collectInlined(a, b, c) { return [a, b, c]; }
function method() { return this.tag + "|" + Array.prototype.join.call(arguments, ","); }

function refCall(fn, thisValue, prefix, spreadArrays, suffix) {
    var args = prefix.slice();
    for (var i = 0; i < spreadArrays.length; ++i)
        for (var j = 0; j < spreadArrays[i].length; ++j)
            args.push(spreadArrays[i][j]);
    for (var i = 0; i < suffix.length; ++i)
        args.push(suffix[i]);
    return fn.apply(thisValue, args);
}
noInline(refCall);

function prefixSpread(x, arr) { return sum5(x, ...arr); }
noInline(prefixSpread);
function spreadSuffix(arr, z) { return sum5(...arr, z); }
noInline(spreadSuffix);
function midSpread(x, arr, z) { return sum5(x, ...arr, z); }
noInline(midSpread);
function twoSpread(a, b) { return sum5(...a, ...b); }
noInline(twoSpread);
function tinyPrefixSpread(x, arr) { return tiny(x, ...arr); }
noInline(tinyPrefixSpread);

var empty = [];
var one = [10];
var many = [1, 2, 3, 4, 5];
var mixed = [7, 8];

for (var i = 0; i < testLoopCount; ++i) {
    assert(prefixSpread(100, empty) === refCall(sum5, undefined, [100], [empty], []), "prefixSpread empty");
    assert(prefixSpread(100, one) === refCall(sum5, undefined, [100], [one], []), "prefixSpread one");
    assert(prefixSpread(100, many) === refCall(sum5, undefined, [100], [many], []), "prefixSpread many");
    assert(spreadSuffix(mixed, 100) === refCall(sum5, undefined, [], [mixed], [100]), "spreadSuffix");
    assert(midSpread(1, mixed, 100) === refCall(sum5, undefined, [1], [mixed], [100]), "midSpread");
    assert(twoSpread(one, mixed) === refCall(sum5, undefined, [], [one, mixed], []), "twoSpread");
    assert(twoSpread(empty, empty) === refCall(sum5, undefined, [], [empty, empty], []), "twoSpread empty");
    assert(tinyPrefixSpread(5, one) === refCall(tiny, undefined, [5], [one], []), "tinyPrefixSpread");
}

// Inlined callee returns an array to check argument identity/order.
function collectPrefixSpread(a, arr) { return collectInlined(a, ...arr); }
noInline(collectPrefixSpread);
for (var i = 0; i < testLoopCount; ++i) {
    assert(arrEq(collectPrefixSpread("a", ["b"]), ["a", "b", undefined]), "collect one");
    assert(arrEq(collectPrefixSpread("a", ["b", "c"]), ["a", "b", "c"]), "collect two");
    assert(arrEq(collectPrefixSpread("a", ["b", "c", "d"]), ["a", "b", "c"]), "collect overflow-params");
    assert(arrEq(collectPrefixSpread("a", []), ["a", undefined, undefined]), "collect empty");
}

// 'this' must be forwarded for an inlined method call with prefix + spread.
var obj = { tag: "T", m: method };
function callMethod(a, arr) { return obj.m(a, ...arr); }
noInline(callMethod);
for (var i = 0; i < testLoopCount; ++i)
    assert(callMethod(1, [2, 3]) === "T|1,2,3", "method this + prefix spread");

// Rest parameter forwarded into a fixed-prefix spread call.
function restForward(x, ...args) { return sum5(x, ...args, 9); }
noInline(restForward);
for (var i = 0; i < testLoopCount; ++i) {
    assert(restForward(1, 2, 3) === refCall(sum5, undefined, [1], [[2, 3]], [9]), "restForward 3");
    assert(restForward(1) === refCall(sum5, undefined, [1], [[]], [9]), "restForward empty rest");
}

// Dynamic spread lengths across iterations.
function dynLen(a, arr) { return collectInlined(a, ...arr); }
noInline(dynLen);
for (var i = 0; i < testLoopCount; ++i) {
    var n = i % 4;
    var a = [];
    for (var j = 0; j < n; ++j) a.push(j + 1);
    var got = dynLen(0, a);
    var want = [0].concat(a).slice(0, 3);
    while (want.length < 3) want.push(undefined);
    assert(arrEq(got, want), "dynLen n=" + n);
}

// OSR-exit path: after tier-up, force a speculation failure while interleaved args are live.
function observe(x) { return x; }
noInline(observe);
function fooExit(a, arr) {
    observe(a);
    return collectInlined(a, ...arr);
}
noInline(fooExit);
for (var i = 0; i < testLoopCount; ++i) {
    var r = fooExit(i, [i + 1]);
    assert(arrEq(r, [i, i + 1, undefined]), "fooExit warmup");
}
var marker = {};
var rExit = fooExit(marker, [20]);
assert(rExit.length === 3 && rExit[0] === marker && rExit[1] === 20 && rExit[2] === undefined, "fooExit exit result");

// Exceptions thrown by the inlined callee must propagate.
function throwerInlined(a) { if (typeof a === "object") throw new Error("boom"); return a; }
function callThrower(a, arr) { return throwerInlined(a, ...arr); }
noInline(callThrower);
for (var i = 0; i < testLoopCount; ++i)
    assert(callThrower(1, many) === 1, "inlined callee returns normally for a non-object argument");
var threwObj = false;
try { callThrower({}, many); } catch (e) { threwObj = e.message === "boom"; }
assert(threwObj, "exception propagation from inlined callee");

// Rest parameter of an inlined wrapper spread into another inlined call; ArgumentsElimination
// forwards operands directly with no rest-array allocation.
function sumForward(a, b, c, d) { return (a | 0) + (b | 0) + (c | 0) + (d | 0); }
function innerRest(x, ...rest) { return sumForward(x, ...rest); }
function outerStatic(a, b) { return innerRest(a, b, 3); }
noInline(outerStatic);
for (var i = 0; i < testLoopCount; ++i)
    assert(outerStatic(1, 2) === 6, "static forward");

// Constant-array spread (PhantomNewArrayBuffer) forwarded into an inlined call.
function constSpread(a) { return sumForward(a, ...[10, 20]); }
noInline(constSpread);
for (var i = 0; i < testLoopCount; ++i)
    assert(constSpread(1) === 31, "const-array spread forward");

var huge = new Array(1000000).fill(1);
var gotRangeError = false;
try {
    (function rec(n) { return sum5(n, ...huge) + rec(n + 1); })(0);
} catch (e) {
    gotRangeError = e instanceof RangeError;
}
assert(gotRangeError, "stack overflow throws RangeError");
