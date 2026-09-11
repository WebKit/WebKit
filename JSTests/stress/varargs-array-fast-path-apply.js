// Regression test for the small-array varargs fast path (Baseline/DFG/FTL) via Function.apply,
// Reflect.apply, and a Proxy apply trap, across arrays that take the fast path and arrays that bail it.

function assert(b, m) {
    if (!b)
        throw new Error("FAIL: " + m);
}
noInline(assert);

function callee() {
    return this.tag + "|" + arguments.length + "|" + Array.prototype.join.call(arguments, ",");
}
noInline(callee);

function viaApply(recv, args) { return callee.apply(recv, args); }
noInline(viaApply);
function viaReflect(recv, args) { return Reflect.apply(callee, recv, args); }
noInline(viaReflect);

var recv = { tag: "T" };

// Fast-path shapes (small dense Int32/Contiguous).
var small0 = [];
var small1 = ["x"];
var small2 = ["x", 1];
var small3 = [true, null, "z"];

// Bail shapes.
var big = []; for (var i = 0; i < 20; ++i) big.push(i);
var holey = [1, , 3];
var dbl = [1.5, 2.5, 3.5];
var argsObject = (function () { return arguments; })(9, 8, 7);

function bigExpect() { var s = "T|20|"; for (var i = 0; i < 20; ++i) s += (i ? "," : "") + i; return s; }

for (var i = 0; i < testLoopCount; ++i) {
    assert(viaApply(recv, small0) === "T|0|", "apply small0");
    assert(viaApply(recv, small1) === "T|1|x", "apply small1");
    assert(viaApply(recv, small2) === "T|2|x,1", "apply small2");
    assert(viaApply(recv, small3) === "T|3|true,,z", "apply small3");
    assert(viaReflect(recv, small2) === "T|2|x,1", "reflect small2");

    assert(viaApply(recv, big) === bigExpect(), "apply big (bail: too long)");
    assert(viaApply(recv, holey) === "T|3|1,,3", "apply holey (bail: hole)");
    assert(viaApply(recv, dbl) === "T|3|1.5,2.5,3.5", "apply dbl (bail: double)");
    assert(viaApply(recv, argsObject) === "T|3|9,8,7", "apply argsObject (bail: non-array)");
}

var target = function (x, y) { return this.base + (x | 0) + (y | 0); };
var proxy = new Proxy(target, {
    apply(t, thisArg, argsList) { return Reflect.apply(t, thisArg, argsList) + 1; }
});
var thisArg = { base: 100 };
function callProxy(args) { return proxy.apply(thisArg, args); }
noInline(callProxy);
for (var i = 0; i < testLoopCount; ++i)
    assert(callProxy([3, 4]) === 108, "proxy apply forwards this + args");
