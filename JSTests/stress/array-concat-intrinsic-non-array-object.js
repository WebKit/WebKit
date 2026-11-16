// Regression test for a bug where tryConcatOneArgFast fell through to the JSArray
// uncheckedDowncast branch for a non-array object argument, dereferencing a bogus butterfly
// pointer and crashing.
//
// The fix: the non-array-object branch must return the concatAppendOne result directly
// (RELEASE_AND_RETURN), not merely check the exception scope and fall through.

function assert(cond, msg) {
    if (!cond)
        throw new Error("Bad: " + (msg || ""));
}
noInline(assert);

function objConcat(a, b) { return a.concat(b); }
noInline(objConcat);

function runAll() {
    // Plain object (not an array) — hits the non-array non-spreadable branch.
    {
        const plainObj = { foo: "bar" };
        const r = objConcat([1, 2], plainObj);
        assert(r.length === 3, "plain-object length " + r.length);
        assert(r[0] === 1);
        assert(r[1] === 2);
        assert(r[2] === plainObj, "plain-object should be single-element-appended");
    }

    // Object with array-like shape but no Symbol.isConcatSpreadable — also non-spreadable.
    {
        const arrLike = { 0: "a", 1: "b", length: 2 };
        const r = objConcat([10, 20], arrLike);
        assert(r.length === 3);
        assert(r[2] === arrLike, "array-like without spreadable must be appended, not spread");
    }

    // Object with prototype set to Array.prototype but not a JSArray — still non-spreadable
    // per `arrayMissingIsConcatSpreadable` semantics (its prototype is array-prototype but
    // isJSArray returns false, so we treat it as a non-array object).
    {
        const pseudoArr = Object.create(Array.prototype);
        pseudoArr[0] = "x"; pseudoArr[1] = "y"; pseudoArr.length = 2;
        const r = objConcat([1], pseudoArr);
        assert(r.length === 2);
        assert(r[1] === pseudoArr);
    }

    // Null and undefined arguments — primitives, not objects.
    {
        const r1 = objConcat([1, 2], null);
        assert(r1.length === 3 && r1[2] === null);
        const r2 = objConcat([1, 2], undefined);
        assert(r2.length === 3 && r2[2] === undefined);
    }
}

// Run at LLInt tier first (where the crash originally reproduced), then warm up through
// Baseline/DFG/FTL to exercise every JIT path.
for (let i = 0; i < testLoopCount; i++)
    runAll();
