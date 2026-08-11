//@ requireOptions("--validateGraph=true")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`FAIL: ${msg}: expected ${expected}, got ${actual}`);
}

function shouldThrow(fn, errorType, msg) {
    let threw = false;
    try {
        fn();
    } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error(`FAIL: ${msg}: expected ${errorType.name}, got ${e.constructor.name}`);
    }
    if (!threw)
        throw new Error(`FAIL: ${msg}: expected throw, got no throw`);
}

// =========================================================================
// Masquerades-as-undefined: non-nullish for ??, but falsy for ToBoolean.
// The condition-context path tests the same value with both
// is_undefined_or_null and a truthiness branch — they must disagree here.
// =========================================================================

const masquerader = makeMasquerader();

function testMasqLhs(a, b) {
    if (a ?? b)
        return "T";
    return "F";
}
noInline(testMasqLhs);

function testMasqRhs(a, b) {
    if (a ?? b)
        return "T";
    return "F";
}
noInline(testMasqRhs);

function testMasqNot(a, b) {
    if (!(a ?? b))
        return "T";
    return "F";
}
noInline(testMasqNot);

function testMasqWhile(state) {
    let c = 0;
    while (state.values[state.i++] ?? false)
        c++;
    return c;
}
noInline(testMasqWhile);

for (let i = 0; i < 1e4; i++) {
    // Masquerader is NOT nullish: rhs must be skipped, condition is falsy.
    shouldBe(testMasqLhs(masquerader, 1), "F", "masq lhs: non-nullish but falsy");
    shouldBe(testMasqRhs(null, masquerader), "F", "masq rhs: falsy");
    shouldBe(testMasqNot(masquerader, 1), "T", "masq under !");
    shouldBe(testMasqWhile({ values: [1, 1, masquerader, 1], i: 0 }), 2, "masq stops while");
}

// Pollute profiling with normal values first, then feed the masquerader to
// force a speculation failure in the optimizing tiers.
function testMasqOSR(a, b) {
    if (a ?? b)
        return "T";
    return "F";
}
noInline(testMasqOSR);

for (let i = 0; i < 1e4; i++)
    shouldBe(testMasqOSR({}, 0), "T", "masqOSR warmup");
for (let i = 0; i < 100; i++)
    shouldBe(testMasqOSR(masquerader, 1), "F", "masqOSR after tier-up");
for (let i = 0; i < 100; i++)
    shouldBe(testMasqOSR(null, masquerader), "F", "masqOSR rhs after tier-up");

// =========================================================================
// lhs must be evaluated exactly once, even though the condition-context
// path inspects the resulting value twice (nullish check + truthiness).
// =========================================================================

let getterCount = 0;
const counting = {
    get x() {
        getterCount++;
        return this.val;
    },
    val: undefined,
};

function testGetterOnce(o, b) {
    if (o.x ?? b)
        return "T";
    return "F";
}
noInline(testGetterOnce);

for (let i = 0; i < 1e4; i++) {
    getterCount = 0;
    counting.val = 1;
    shouldBe(testGetterOnce(counting, 0), "T", "getter: truthy");
    shouldBe(getterCount, 1, "getter called once (truthy)");

    getterCount = 0;
    counting.val = 0;
    shouldBe(testGetterOnce(counting, 1), "F", "getter: falsy non-nullish");
    shouldBe(getterCount, 1, "getter called once (falsy non-nullish)");

    getterCount = 0;
    counting.val = undefined;
    shouldBe(testGetterOnce(counting, 1), "T", "getter: nullish");
    shouldBe(getterCount, 1, "getter called once (nullish)");
}

let callCount = 0;
function testCallOnce(o, b) {
    if (o?.f() ?? b)
        return "T";
    return "F";
}
noInline(testCallOnce);

for (let i = 0; i < 1e4; i++) {
    callCount = 0;
    shouldBe(testCallOnce({ f() { callCount++; return 0; } }, 1), "F", "call: falsy non-nullish");
    shouldBe(callCount, 1, "f called once");

    callCount = 0;
    shouldBe(testCallOnce(null, 1), "T", "call: chain short-circuit");
    shouldBe(callCount, 0, "f not called on short-circuit");
}

// =========================================================================
// rhs must NOT be evaluated when lhs is non-nullish — including every
// falsy non-nullish value, where a buggy lowering might fall into the
// rhs path.
// =========================================================================

let rhsCount = 0;
function rhsEffect() {
    rhsCount++;
    return 1;
}

function testRhsSkipped(a) {
    if (a ?? rhsEffect())
        return "T";
    return "F";
}
noInline(testRhsSkipped);

const falsyNonNullish = [0, -0, "", false, NaN, 0n, masquerader];
for (let i = 0; i < 1e3; i++) {
    for (const v of falsyNonNullish) {
        rhsCount = 0;
        shouldBe(testRhsSkipped(v), "F", `rhs skipped: ${String(v)} is falsy`);
        shouldBe(rhsCount, 0, `rhs not evaluated for ${String(v)}`);
    }
    rhsCount = 0;
    shouldBe(testRhsSkipped(1), "T", "rhs skipped: truthy");
    shouldBe(rhsCount, 0, "rhs not evaluated for truthy");

    rhsCount = 0;
    shouldBe(testRhsSkipped(null), "T", "rhs evaluated for null");
    shouldBe(rhsCount, 1, "rhs evaluated once for null");

    rhsCount = 0;
    shouldBe(testRhsSkipped(undefined), "T", "rhs evaluated for undefined");
    shouldBe(rhsCount, 1, "rhs evaluated once for undefined");
}

// =========================================================================
// ToBoolean must not invoke valueOf / toString / Symbol.toPrimitive.
// =========================================================================

const trapped = {
    valueOf() { throw new Error("valueOf must not be called"); },
    toString() { throw new Error("toString must not be called"); },
    [Symbol.toPrimitive]() { throw new Error("toPrimitive must not be called"); },
};

function testNoToPrimitive(a, b) {
    if (a ?? b)
        return "T";
    return "F";
}
noInline(testNoToPrimitive);

for (let i = 0; i < 1e3; i++) {
    shouldBe(testNoToPrimitive(trapped, 0), "T", "object lhs truthy without ToPrimitive");
    shouldBe(testNoToPrimitive(null, trapped), "T", "object rhs truthy without ToPrimitive");
}

// =========================================================================
// Primitive truthiness coverage on both paths
// =========================================================================

function testPrim(a, b) {
    if (a ?? b)
        return "T";
    return "F";
}
noInline(testPrim);

for (let i = 0; i < 1e3; i++) {
    shouldBe(testPrim(0n, 1), "F", "0n falsy non-nullish");
    shouldBe(testPrim(1n, 0), "T", "1n truthy");
    shouldBe(testPrim(null, 0n), "F", "rhs 0n");
    shouldBe(testPrim(null, 1n), "T", "rhs 1n");
    shouldBe(testPrim(Symbol(), 0), "T", "symbol truthy");
    shouldBe(testPrim(null, Symbol()), "T", "rhs symbol truthy");
    shouldBe(testPrim(-0, 1), "F", "-0 falsy non-nullish");
    shouldBe(testPrim("0", 0), "T", "'0' truthy");
}

// =========================================================================
// Assignment as lhs: side effect must land exactly once, value flows to
// the condition.
// =========================================================================

function testAssignLhs(v, b) {
    let x = "unset";
    let r;
    if ((x = v) ?? b)
        r = "T";
    else
        r = "F";
    return `${r}:${String(x)}`;
}
noInline(testAssignLhs);

for (let i = 0; i < 1e3; i++) {
    shouldBe(testAssignLhs(1, 0), "T:1", "assign lhs truthy");
    shouldBe(testAssignLhs(0, 1), "F:0", "assign lhs falsy non-nullish");
    shouldBe(testAssignLhs(null, 1), "T:null", "assign lhs null, rhs truthy");
    shouldBe(testAssignLhs(null, 0), "F:null", "assign lhs null, rhs falsy");
}

// =========================================================================
// Left-associative nesting with absorbed optional chains at each level
// (chain target stack must stay balanced).
// =========================================================================

function testNestedChains(p, q, r) {
    if ((p?.x ?? q?.y) ?? r?.z)
        return "T";
    return "F";
}
noInline(testNestedChains);

for (let i = 0; i < 1e4; i++) {
    shouldBe(testNestedChains({ x: 1 }, null, null), "T", "nested chains: p.x");
    shouldBe(testNestedChains({ x: 0 }, { y: 1 }, { z: 1 }), "F", "nested chains: p.x falsy non-nullish");
    shouldBe(testNestedChains(null, { y: 1 }, null), "T", "nested chains: q.y");
    shouldBe(testNestedChains(null, { y: 0 }, { z: 1 }), "F", "nested chains: q.y falsy non-nullish");
    shouldBe(testNestedChains(null, null, { z: 1 }), "T", "nested chains: r.z");
    shouldBe(testNestedChains(null, null, { z: 0 }), "F", "nested chains: r.z falsy");
    shouldBe(testNestedChains(null, null, null), "F", "nested chains: all nullish");
}

// `this` binding through an absorbed chain call.
function testThisBinding(o, b) {
    if (o?.m() ?? b)
        return "T";
    return "F";
}
noInline(testThisBinding);

const thisObj = {
    m() {
        return this === thisObj;
    },
};

for (let i = 0; i < 1e3; i++) {
    shouldBe(testThisBinding(thisObj, 0), "T", "this preserved through absorbed chain");
    shouldBe(testThisBinding(null, 0), "F", "chain short-circuit to rhs");
}

// delete inside an optional chain as lhs.
function testDeleteChain(o, b) {
    if ((delete o?.x) ?? b)
        return "T";
    return "F";
}
noInline(testDeleteChain);

for (let i = 0; i < 1e3; i++) {
    shouldBe(testDeleteChain({ x: 1 }, 0), "T", "delete o?.x is true");
    shouldBe(testDeleteChain(null, 0), "T", "delete with nullish base is true");
}

// =========================================================================
// Exception from lhs evaluation must propagate without touching rhs.
// =========================================================================

function testThrowingGetter(o) {
    if (o.x ?? rhsEffect())
        return "T";
    return "F";
}
noInline(testThrowingGetter);

const throwing = {
    get x() { throw new TypeError("boom"); },
};

for (let i = 0; i < 1e3; i++) {
    rhsCount = 0;
    shouldThrow(() => testThrowingGetter(throwing), TypeError, "getter throws");
    shouldBe(rhsCount, 0, "rhs untouched when lhs throws");
}

// =========================================================================
// Scope-sensitive lhs: with-scope and direct eval.
// =========================================================================

function testWithScope(scope, b) {
    with (scope) {
        if (x ?? b)
            return "T";
        return "F";
    }
}
noInline(testWithScope);

for (let i = 0; i < 1e3; i++) {
    shouldBe(testWithScope({ x: 1 }, 0), "T", "with: truthy");
    shouldBe(testWithScope({ x: 0 }, 1), "F", "with: falsy non-nullish");
    shouldBe(testWithScope({ x: null }, 1), "T", "with: nullish, rhs truthy");
}

function testDirectEval(a, b) {
    return eval("if (a ?? b) 'T'; else 'F';");
}
noInline(testDirectEval);

for (let i = 0; i < 100; i++) {
    shouldBe(testDirectEval(0, 1), "F", "eval: falsy non-nullish");
    shouldBe(testDirectEval(null, 1), "T", "eval: rhs truthy");
    shouldBe(testDirectEval(masquerader, 1), "F", "eval: masquerader");
}

// =========================================================================
// Register pressure: condition emitted while many temporaries are live,
// rhs call clobbers caller-side temps.
// =========================================================================

function clobber(a, b, c, d, e, f, g, h) {
    return a + b + c + d + e + f + g + h;
}
noInline(clobber);

function testRegisterPressure(v, w) {
    const t1 = v + 1, t2 = v + 2, t3 = v + 3, t4 = v + 4;
    const t5 = v + 5, t6 = v + 6, t7 = v + 7, t8 = v + 8;
    let r;
    if (w ?? clobber(t1, t2, t3, t4, t5, t6, t7, t8))
        r = "T";
    else
        r = "F";
    return `${r}:${t1 + t2 + t3 + t4 + t5 + t6 + t7 + t8}`;
}
noInline(testRegisterPressure);

for (let i = 0; i < 1e4; i++) {
    shouldBe(testRegisterPressure(0, null), "T:36", "pressure: rhs call truthy");
    shouldBe(testRegisterPressure(0, 0), "F:36", "pressure: lhs falsy non-nullish");
    shouldBe(testRegisterPressure(0, 1), "T:36", "pressure: lhs truthy");
}

// =========================================================================
// Composition with conditional expression in a loop condition.
// =========================================================================

function testComposed(state) {
    let n = 0;
    while ((state.v ?? state.fallback) ? state.count-- > 0 : false)
        n++;
    return n;
}
noInline(testComposed);

for (let i = 0; i < 1e3; i++) {
    shouldBe(testComposed({ v: true, fallback: false, count: 3 }), 3, "composed: lhs truthy");
    shouldBe(testComposed({ v: null, fallback: true, count: 2 }), 2, "composed: rhs truthy");
    shouldBe(testComposed({ v: null, fallback: false, count: 5 }), 0, "composed: rhs falsy");
    shouldBe(testComposed({ v: masquerader, fallback: true, count: 5 }), 0, "composed: masquerader falsy");
}
