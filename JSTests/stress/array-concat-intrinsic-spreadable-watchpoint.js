// Tests that Array.prototype.concat correctly handles invalidation of the
// arrayIsConcatSpreadableWatchpointSet. The DFG intrinsic installs this watchpoint lazily;
// any addition of Symbol.isConcatSpreadable to Array.prototype or Object.prototype must
// invalidate compiled code so that subsequent compilations/executions produce spec-compliant
// results.

function assert(cond, msg) {
    if (!cond)
        throw new Error("Bad: " + (msg || ""));
}
noInline(assert);

function shallowEq(a, b) {
    assert(a.length === b.length, "length: " + a.length + " vs " + b.length);
    for (let i = 0; i < a.length; i++)
        assert(a[i] === b[i], "index " + i + ": " + a[i] + " vs " + b[i]);
}
noInline(shallowEq);

function concatArr(a, b) { return a.concat(b); }
noInline(concatArr);

function concatOne(a, v) { return a.concat(v); }
noInline(concatOne);

// Phase 1: with the watchpoint still valid, warm up both shapes so the intrinsic fires
// and fixup promotes to ArrayConcatArray / narrows to NotCellUse.
for (let i = 0; i < testLoopCount; i++) {
    shallowEq(concatArr([1, 2, 3], [4, 5]), [1, 2, 3, 4, 5]);
    shallowEq(concatOne([1, 2], 42), [1, 2, 42]);
}

// Phase 2: invalidate arrayIsConcatSpreadableWatchpointSet by setting Symbol.isConcatSpreadable
// on Array.prototype. Per ECMA-262 IsConcatSpreadable, the inherited value overrides the
// default "IsArray" fallback. Setting it to `false` makes arrays no longer auto-spread —
// the spec applies the check to both the receiver and each argument.
Array.prototype[Symbol.isConcatSpreadable] = false;

// After invalidation the cached compiled code is no longer valid. Runs must observe the
// updated semantics: both the receiver and the argument are treated as single elements.
for (let i = 0; i < testLoopCount; i++) {
    let a = [1, 2, 3];
    let b = [4, 5];
    let r = concatArr(a, b);
    // Expected: [a, b], length 2 — both receiver and argument are non-spreadable.
    assert(r.length === 2, "after invalidation arr length " + r.length);
    assert(r[0] === a, "after invalidation r[0] should be a");
    assert(r[1] === b, "after invalidation r[1] should be b");

    // For primitive args the result depends on the receiver's spreadability. With the
    // receiver also non-spreadable (inherited false), result is [[1,2], 42] — length 2.
    let r2 = concatOne([1, 2], 42);
    assert(r2.length === 2, "after invalidation one length " + r2.length);
    assert(Array.isArray(r2[0]) && r2[0][0] === 1 && r2[0][1] === 2, "r2[0] wrong");
    assert(r2[1] === 42, "r2[1] wrong");
}

// Phase 3: set it back to the default (true) on Array.prototype. The watchpoint stays
// invalidated (it's one-shot), but the per-instance override should propagate via the
// prototype chain the same way.
Array.prototype[Symbol.isConcatSpreadable] = true;
for (let i = 0; i < testLoopCount; i++) {
    shallowEq(concatArr([1, 2, 3], [4, 5]), [1, 2, 3, 4, 5]);
    shallowEq(concatOne([1, 2], 42), [1, 2, 42]);
}

// Clean up to avoid leaking state to any subsequent test loader.
delete Array.prototype[Symbol.isConcatSpreadable];
