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
// Basic patterns
// =========================================================================

function testIf(a, b) {
    if (a ?? b)
        return "T";
    return "F";
}
noInline(testIf);

function testNot(a, b) {
    if (!(a ?? b))
        return "T";
    return "F";
}
noInline(testNot);

function testRhsAnd(a, b, c) {
    if (a ?? (b && c))
        return "T";
    return "F";
}
noInline(testRhsAnd);

function testRhsOr(a, b, c) {
    if (a ?? (b || c))
        return "T";
    return "F";
}
noInline(testRhsOr);

function testRhsNot(a, b) {
    if (a ?? !b)
        return "T";
    return "F";
}
noInline(testRhsNot);

function testRhsRel(a, b, c) {
    if (a ?? b < c)
        return "T";
    return "F";
}
noInline(testRhsRel);

function testRhsEq(a, b, c) {
    if (a ?? b === c)
        return "T";
    return "F";
}
noInline(testRhsEq);

function testNested(a, b, c) {
    if (a ?? (b ?? c))
        return "T";
    return "F";
}
noInline(testNested);

function testTernaryCond(a, b) {
    return (a ?? b) ? "T" : "F";
}
noInline(testTernaryCond);

function testOuterAnd(a, b, c) {
    if ((a ?? b) && c)
        return "T";
    return "F";
}
noInline(testOuterAnd);

function testOuterOr(a, b, c) {
    if ((a ?? b) || c)
        return "T";
    return "F";
}
noInline(testOuterOr);

function testWhile(state) {
    let c = 0;
    while (state.v ?? state.next())
        c++;
    return c;
}
noInline(testWhile);

function testFor(arr) {
    let log = [];
    for (let i = 0; arr[i] ?? false; i++)
        log.push(arr[i]);
    return log.join(",");
}
noInline(testFor);

function testDoWhile(state) {
    let c = 0;
    do {
        c++;
    } while (state.v ?? --state.n > 0);
    return c;
}
noInline(testDoWhile);

function testCommaLhs(a, b, c) {
    if ((a, b ?? c))
        return "T";
    return "F";
}
noInline(testCommaLhs);

// =========================================================================
// Run basic patterns
// =========================================================================

for (let i = 0; i < 1e4; i++) {
    shouldBe(testIf(1, 0), "T", "testIf(1,0): lhs non-nullish truthy");
    shouldBe(testIf(0, 1), "F", "testIf(0,1): lhs non-nullish falsy");
    shouldBe(testIf(null, 1), "T", "testIf(null,1): rhs truthy");
    shouldBe(testIf(null, 0), "F", "testIf(null,0): rhs falsy");
    shouldBe(testIf(undefined, "x"), "T", "testIf(undef,'x')");
    shouldBe(testIf(undefined, NaN), "F", "testIf(undef,NaN)");
    shouldBe(testIf("", 1), "F", "testIf('',1): empty string non-nullish falsy");
    shouldBe(testIf(false, 1), "F", "testIf(false,1): false non-nullish");
    shouldBe(testIf(NaN, 1), "F", "testIf(NaN,1): NaN non-nullish falsy");

    shouldBe(testNot(1, 0), "F", "testNot(1,0)");
    shouldBe(testNot(0, 1), "T", "testNot(0,1)");
    shouldBe(testNot(null, 0), "T", "testNot(null,0)");
    shouldBe(testNot(null, 1), "F", "testNot(null,1)");

    shouldBe(testRhsAnd(1, 0, 0), "T", "rhsAnd: lhs truthy");
    shouldBe(testRhsAnd(0, 1, 1), "F", "rhsAnd: lhs falsy non-nullish");
    shouldBe(testRhsAnd(null, 1, 1), "T", "rhsAnd: b&&c true");
    shouldBe(testRhsAnd(null, 0, 1), "F", "rhsAnd: b falsy");
    shouldBe(testRhsAnd(null, 1, 0), "F", "rhsAnd: c falsy");

    shouldBe(testRhsOr(null, 0, 1), "T", "rhsOr: c truthy");
    shouldBe(testRhsOr(null, 0, 0), "F", "rhsOr: both falsy");
    shouldBe(testRhsOr(0, 1, 1), "F", "rhsOr: lhs falsy non-nullish");

    shouldBe(testRhsNot(null, 0), "T", "rhsNot: !0");
    shouldBe(testRhsNot(null, 1), "F", "rhsNot: !1");
    shouldBe(testRhsNot(false, 0), "F", "rhsNot: lhs false");

    shouldBe(testRhsRel(null, 1, 2), "T", "rhsRel: 1<2");
    shouldBe(testRhsRel(null, 2, 1), "F", "rhsRel: 2<1");
    shouldBe(testRhsEq(null, 1, 1), "T", "rhsEq: 1===1");
    shouldBe(testRhsEq(null, 1, 2), "F", "rhsEq: 1!==2");

    shouldBe(testNested(null, null, 1), "T", "nested: innermost truthy");
    shouldBe(testNested(null, null, 0), "F", "nested: innermost falsy");
    shouldBe(testNested(null, 1, 0), "T", "nested: middle truthy");
    shouldBe(testNested(0, 1, 1), "F", "nested: outer falsy non-nullish");

    shouldBe(testTernaryCond(1, 0), "T", "ternaryCond lhs truthy");
    shouldBe(testTernaryCond(null, 1), "T", "ternaryCond rhs truthy");
    shouldBe(testTernaryCond(null, 0), "F", "ternaryCond rhs falsy");

    shouldBe(testOuterAnd(1, 0, 1), "T", "outerAnd: lhs truthy, c truthy");
    shouldBe(testOuterAnd(null, 1, 1), "T", "outerAnd: rhs truthy, c truthy");
    shouldBe(testOuterAnd(null, 1, 0), "F", "outerAnd: c falsy");
    shouldBe(testOuterAnd(0, 1, 1), "F", "outerAnd: lhs falsy non-nullish");

    shouldBe(testOuterOr(0, 1, 0), "F", "outerOr: lhs falsy non-nullish, c falsy");
    shouldBe(testOuterOr(null, 0, 1), "T", "outerOr: c truthy");
    shouldBe(testOuterOr(1, 0, 0), "T", "outerOr: lhs truthy");

    shouldBe(testWhile({ v: null, n: 0, next() { return this.n++ < 3; } }), 3, "testWhile");
    shouldBe(testFor([1, 2, 3, null]), "1,2,3", "testFor stops at null");
    shouldBe(testFor([1, 2, 0, 3]), "1,2", "testFor stops at falsy non-nullish");
    shouldBe(testDoWhile({ v: null, n: 3 }), 3, "testDoWhile");
    shouldBe(testDoWhile({ v: false, n: 3 }), 1, "testDoWhile lhs false");

    shouldBe(testCommaLhs(99, null, 1), "T", "commaLhs: rhs truthy");
    shouldBe(testCommaLhs(99, null, 0), "F", "commaLhs: rhs falsy");
    shouldBe(testCommaLhs(99, 1, 0), "T", "commaLhs: lhs truthy");
}

// =========================================================================
// Optional chain absorption: a?.b ?? c in condition context
// =========================================================================

function testOptChain(o, b) {
    if (o?.x ?? b)
        return "T";
    return "F";
}
noInline(testOptChain);

function testOptChainCall(o, b) {
    if (o?.f() ?? b)
        return "T";
    return "F";
}
noInline(testOptChainCall);

function testOptChainDeep(o, b) {
    if (o?.x?.y ?? b)
        return "T";
    return "F";
}
noInline(testOptChainDeep);

function testOptChainRhsAnd(o, b, c) {
    if (o?.x ?? (b && c))
        return "T";
    return "F";
}
noInline(testOptChainRhsAnd);

for (let i = 0; i < 1e4; i++) {
    shouldBe(testOptChain({ x: 1 }, 0), "T", "optChain: x truthy");
    shouldBe(testOptChain({ x: 0 }, 1), "F", "optChain: x falsy non-nullish");
    shouldBe(testOptChain({ x: null }, 1), "T", "optChain: x nullish, b truthy");
    shouldBe(testOptChain(null, 1), "T", "optChain: o nullish, b truthy");
    shouldBe(testOptChain(null, 0), "F", "optChain: o nullish, b falsy");
    shouldBe(testOptChain(undefined, 0), "F", "optChain: o undefined, b falsy");
    shouldBe(testOptChain({}, 1), "T", "optChain: x undefined, b truthy");

    shouldBe(testOptChainCall({ f() { return 1; } }, 0), "T", "optChainCall: returns truthy");
    shouldBe(testOptChainCall({ f() { return 0; } }, 1), "F", "optChainCall: returns falsy");
    shouldBe(testOptChainCall(null, 1), "T", "optChainCall: o nullish");
    shouldBe(testOptChainCall(null, 0), "F", "optChainCall: o nullish, b falsy");

    shouldBe(testOptChainDeep({ x: { y: 1 } }, 0), "T", "optChainDeep: y truthy");
    shouldBe(testOptChainDeep({ x: null }, 1), "T", "optChainDeep: x nullish");
    shouldBe(testOptChainDeep(null, 0), "F", "optChainDeep: o nullish, b falsy");

    shouldBe(testOptChainRhsAnd(null, 1, 1), "T", "optChainRhsAnd: b&&c");
    shouldBe(testOptChainRhsAnd(null, 1, 0), "F", "optChainRhsAnd: c falsy");
    shouldBe(testOptChainRhsAnd({ x: 1 }, 0, 0), "T", "optChainRhsAnd: x truthy");
}

// =========================================================================
// Side effect ordering
// =========================================================================

let log = [];
function s(v, tag) {
    log.push(tag);
    return v;
}

function testOrder(a, b) {
    if (s(a, "a") ?? s(b, "b"))
        return "T";
    return "F";
}
noInline(testOrder);

function testOrderShortCircuit(a, b) {
    if (s(a, "a") ?? s(b, "b"))
        return "T";
    return "F";
}
noInline(testOrderShortCircuit);

function testOrderInRhsAnd(a, b, c) {
    if (s(a, "a") ?? (s(b, "b") && s(c, "c")))
        return "T";
    return "F";
}
noInline(testOrderInRhsAnd);

for (let i = 0; i < 1e3; i++) {
    log = [];
    shouldBe(testOrder(null, 1), "T", "order: rhs evaluated");
    shouldBe(log.join(","), "a,b", "order: a then b");

    log = [];
    shouldBe(testOrderShortCircuit(0, 1), "F", "order: lhs non-nullish");
    shouldBe(log.join(","), "a", "order: b skipped when lhs non-nullish");

    log = [];
    shouldBe(testOrderShortCircuit(1, 1), "T", "order: lhs truthy");
    shouldBe(log.join(","), "a", "order: b skipped when lhs truthy");

    log = [];
    shouldBe(testOrderInRhsAnd(null, 1, 1), "T", "orderInRhsAnd: full");
    shouldBe(log.join(","), "a,b,c", "orderInRhsAnd: a,b,c");

    log = [];
    shouldBe(testOrderInRhsAnd(null, 0, 1), "F", "orderInRhsAnd: b falsy");
    shouldBe(log.join(","), "a,b", "orderInRhsAnd: c skipped");
}

// =========================================================================
// Throws
// =========================================================================

function testThrowLhs() {
    if ((() => { throw new Error("lhs"); })() ?? 1)
        return "T";
    return "F";
}
function testThrowRhs() {
    if (null ?? (() => { throw new Error("rhs"); })())
        return "T";
    return "F";
}
noInline(testThrowLhs);
noInline(testThrowRhs);

for (let i = 0; i < 1e3; i++) {
    shouldThrow(() => testThrowLhs(), Error, "throw in lhs");
    shouldThrow(() => testThrowRhs(), Error, "throw in rhs");
}

// =========================================================================
// Generator / async
// =========================================================================

function* gen(a, b) {
    if (a ?? b)
        yield "T";
    else
        yield "F";
}

async function asyncFn(a, b) {
    if (a ?? b)
        return "T";
    return "F";
}

(async () => {
    for (let i = 0; i < 100; i++) {
        shouldBe(gen(1, 0).next().value, "T", "generator lhs truthy");
        shouldBe(gen(null, 0).next().value, "F", "generator rhs falsy");
        shouldBe(gen(null, 1).next().value, "T", "generator rhs truthy");

        shouldBe(await asyncFn(0, 1), "F", "async lhs falsy non-nullish");
        shouldBe(await asyncFn(null, 1), "T", "async rhs truthy");
    }
})();

// =========================================================================
// Switch
// =========================================================================

function testSwitch(a, b) {
    switch (true) {
        case (a ?? b > 5):
            return "big";
        case (a ?? b > 0):
            return "small";
        default:
            return "none";
    }
}
noInline(testSwitch);

for (let i = 0; i < 1e3; i++) {
    shouldBe(testSwitch(null, 10), "big");
    shouldBe(testSwitch(null, 3), "small");
    shouldBe(testSwitch(null, -1), "none");
    shouldBe(testSwitch(true, -1), "big");
}
