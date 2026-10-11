// Regression test for the fused op_call_varargs_with_spread path (f(a, ...b, c)): it must produce
// results identical to the unfused op_new_array_with_spread + op_call_varargs path across all tiers.

function assert(b, msg) {
    if (!b)
        throw new Error("Bad assertion: " + msg);
}
noInline(assert);

function tag(x) {
    if (x === null) return "null";
    if (typeof x === "object" || typeof x === "function") return "obj";
    if (typeof x === "number" && x !== x) return "NaN";
    return typeof x + ":" + String(x);
}
function callee() {
    var parts = [tag(this)];
    for (var i = 0; i < arguments.length; ++i)
        parts.push(tag(arguments[i]));
    return parts.join("|");
}

// Reference: build the merged argument list explicitly (Array.from iterates like op_spread) and apply.
function reference(thisV, prefix, iterable, suffix) {
    var args = prefix.concat(Array.from(iterable)).concat(suffix);
    return callee.apply(thisV, args);
}
noInline(reference);

function fusedPrefixSpread(a, it) { return callee(a, ...it); }
noInline(fusedPrefixSpread);
function fusedSpreadSuffix(it, z) { return callee(...it, z); }
noInline(fusedSpreadSuffix);
function fusedMid(a, it, z) { return callee(a, ...it, z); }
noInline(fusedMid);
function fusedTwo(it1, it2) { return callee(...it1, ...it2); }
noInline(fusedTwo);

function* gen(n) { for (var i = 0; i < n; ++i) yield "g" + i; }

function makeIterables() {
    return [
        [],
        [1, 2, 3],
        [, , 3],                                  // holes -> undefined
        [1.5, 2.5, NaN],                          // ArrayWithDouble
        ["x", null, undefined, true],
        new Set([10, 20, 30]),
        new Map([["k1", 1], ["k2", 2]]),          // yields [key,value] pairs
        "abc",
        "a\u{1F600}b",                            // surrogate pair -> 3 code points
        new Int32Array([7, 8, 9]),
        new Float64Array([1.5, 2.5]),
        gen(4),
        Array.prototype,                          // empty array-like
    ];
}

// Every iterable exercises each fused call once per pass, so scale the pass count to keep the number
// of calls per function at testLoopCount rather than a multiple of it.
var passes = Math.max(1, Math.ceil(testLoopCount / makeIterables().length));
for (var iter = 0; iter < passes; ++iter) {
    var iterables = makeIterables();
    for (var k = 0; k < iterables.length; ++k) {
        var it = iterables[k];
        // Generators are one-shot: snapshot to an array for repeated comparison.
        var snapshot = Array.from(it);
        assert(fusedPrefixSpread("A", snapshot) === reference(undefined, ["A"], snapshot, []), "prefix k=" + k);
        assert(fusedSpreadSuffix(snapshot, "Z") === reference(undefined, [], snapshot, ["Z"]), "suffix k=" + k);
        assert(fusedMid("A", snapshot, "Z") === reference(undefined, ["A"], snapshot, ["Z"]), "mid k=" + k);
        assert(fusedTwo(snapshot, [9, 9]) === reference(undefined, [], snapshot.concat([9, 9]), []), "two k=" + k);
    }
}

// Custom Symbol.iterator must iterate exactly like Array.from.
function customIterable(values) {
    return { [Symbol.iterator]() { var i = 0; return { next() { return i < values.length ? { value: values[i++], done: false } : { value: undefined, done: true }; } }; } };
}
for (var i = 0; i < testLoopCount; ++i) {
    var c = customIterable(["p", "q", "r"]);
    assert(fusedMid("A", c, "Z") === reference(undefined, ["A"], ["p", "q", "r"], ["Z"]), "custom iterator");
}

// Exception thrown mid-iteration: callee must not run and the error must propagate.
function throwingIterable(throwAt) {
    return { [Symbol.iterator]() { var i = 0; return { next() { if (i === throwAt) throw new Error("iter-boom"); return { value: i++, done: false }; } }; } };
}
var calleeRan = false;
function observingCallee() { calleeRan = true; return 0; }
function fusedThrow(it) { return observingCallee(1, ...it, 2); }
noInline(fusedThrow);
for (var i = 0; i < testLoopCount; ++i) {
    calleeRan = false;
    var threw = false;
    try { fusedThrow(throwingIterable(2)); } catch (e) { threw = e.message === "iter-boom"; }
    assert(threw, "exception propagated");
    assert(!calleeRan, "callee must not run when spread throws");
}

// 'this' binding for a method call with prefix + spread.
var obj = { id: "T", m() { return this.id + "#" + callee.apply(this, arguments); } };
function fusedMethod(a, it) { return obj.m(a, ...it); }
noInline(fusedMethod);
for (var i = 0; i < testLoopCount; ++i)
    assert(fusedMethod("A", [1, 2]) === "T#" + callee.call(obj, "A", 1, 2), "method this");

// A huge spread must throw RangeError in the fused path just like apply.
function overflowFused(it) { return callee(1, ...it); }
noInline(overflowFused);
var huge = new Array(1000000).fill(1);
var fusedThrewRange = false;
try { overflowFused(huge); } catch (e) { fusedThrewRange = e instanceof RangeError; }
var applyThrewRange = false;
try { callee.apply(undefined, [1].concat(huge)); } catch (e) { applyThrewRange = e instanceof RangeError; }
assert(fusedThrewRange === applyThrewRange, "overflow parity: fused=" + fusedThrewRange + " apply=" + applyThrewRange);
