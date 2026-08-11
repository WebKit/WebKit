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
    if ((a, b))
        return "T";
    return "F";
}
noInline(testIf);

function testNot(a, b) {
    if (!(a, b))
        return "T";
    return "F";
}
noInline(testNot);

function testAnd(a, b, c) {
    if ((a, b && c))
        return "T";
    return "F";
}
noInline(testAnd);

function testOr(a, b, c) {
    if ((a, b || c))
        return "T";
    return "F";
}
noInline(testOr);

function testNested(a, b, c, d) {
    if ((a, b && (c || d)))
        return "T";
    return "F";
}
noInline(testNested);

function testTernary(a, b, c, d) {
    return (a, b ? c : d);
}
noInline(testTernary);

function testTernaryCond(a, b) {
    return (a, b) ? "T" : "F";
}
noInline(testTernaryCond);

function testMulti(a, b, c, d) {
    if ((a, b, c, d))
        return "T";
    return "F";
}
noInline(testMulti);

function testWhile(state) {
    let c = 0;
    while ((state.tick++, state.n-- > 0))
        c++;
    return c;
}
noInline(testWhile);

function testFor(arr) {
    let log = [];
    for (let i = 0; (log.push("c"), i < arr.length); i++)
        log.push(arr[i]);
    return log.join(",");
}
noInline(testFor);

function testDoWhile(state) {
    let c = 0;
    do {
        c++;
    } while ((state.tick++, --state.n > 0));
    return c;
}
noInline(testDoWhile);

function testRel(a, b, c) {
    if ((a, b < c))
        return "T";
    return "F";
}
noInline(testRel);

function testEq(a, b, c) {
    if ((a, b === c))
        return "T";
    return "F";
}
noInline(testEq);

function testNestedComma(a, b, c) {
    if (((a, b), c))
        return "T";
    return "F";
}
noInline(testNestedComma);

function testOuterAnd(a, b, c) {
    if ((a, b) && c)
        return "T";
    return "F";
}
noInline(testOuterAnd);

function testOuterOr(a, b, c) {
    if ((a, b) || c)
        return "T";
    return "F";
}
noInline(testOuterOr);

function testNullish(a, b, c) {
    if ((a, b) ?? c)
        return "T";
    return "F";
}
noInline(testNullish);

// =========================================================================
// Run basic patterns
// =========================================================================

for (let i = 0; i < 1e4; i++) {
    shouldBe(testIf(0, 1), "T", "testIf(0,1): last truthy");
    shouldBe(testIf(1, 0), "F", "testIf(1,0): last falsy");
    shouldBe(testIf(0, 0), "F", "testIf(0,0)");
    shouldBe(testIf(undefined, "x"), "T", "testIf(undef,'x')");
    shouldBe(testIf(NaN, ""), "F", "testIf(NaN,'')");

    shouldBe(testNot(0, 1), "F", "testNot(0,1)");
    shouldBe(testNot(1, 0), "T", "testNot(1,0)");

    shouldBe(testAnd(99, 1, 1), "T", "testAnd(99,1,1)");
    shouldBe(testAnd(99, 0, 1), "F", "testAnd: short-circuit on b");
    shouldBe(testAnd(99, 1, 0), "F", "testAnd: c falsy");
    shouldBe(testAnd(99, 0, 0), "F", "testAnd: both falsy");

    shouldBe(testOr(99, 0, 1), "T", "testOr: c truthy");
    shouldBe(testOr(99, 1, 0), "T", "testOr: b truthy short-circuit");
    shouldBe(testOr(99, 0, 0), "F", "testOr: both falsy");

    shouldBe(testNested(99, 1, 0, 1), "T", "nested: b && (c||d): d truthy");
    shouldBe(testNested(99, 1, 1, 0), "T", "nested: c truthy");
    shouldBe(testNested(99, 0, 1, 1), "F", "nested: b falsy");
    shouldBe(testNested(99, 1, 0, 0), "F", "nested: c||d both falsy");

    shouldBe(testTernary(99, true, "yes", "no"), "yes", "testTernary truthy");
    shouldBe(testTernary(99, false, "yes", "no"), "no", "testTernary falsy");
    shouldBe(testTernary(99, 0, "yes", "no"), "no", "testTernary 0");

    shouldBe(testTernaryCond(99, 1), "T", "testTernaryCond truthy");
    shouldBe(testTernaryCond(99, 0), "F", "testTernaryCond falsy");

    shouldBe(testMulti(1, 2, 3, 4), "T", "testMulti(1,2,3,4)");
    shouldBe(testMulti(1, 2, 3, 0), "F", "testMulti last 0");
    shouldBe(testMulti(0, 0, 0, 1), "T", "testMulti only last truthy");

    shouldBe(testWhile({ tick: 0, n: 5 }), 5, "testWhile counts down");
    shouldBe(testFor([1, 2, 3]), "c,1,c,2,c,3,c", "testFor with side-effect cond");
    shouldBe(testDoWhile({ tick: 0, n: 3 }), 3, "testDoWhile");

    shouldBe(testRel(99, 1, 2), "T", "testRel 1<2");
    shouldBe(testRel(99, 2, 1), "F", "testRel 2<1");
    shouldBe(testEq(99, 1, 1), "T", "testEq 1===1");
    shouldBe(testEq(99, 1, 2), "F", "testEq 1!==2");

    shouldBe(testNestedComma(0, 0, 1), "T", "((a,b),c): c truthy");
    shouldBe(testNestedComma(1, 1, 0), "F", "((a,b),c): c falsy");

    shouldBe(testOuterAnd(0, 1, 1), "T", "outerAnd both truthy");
    shouldBe(testOuterAnd(0, 0, 1), "F", "outerAnd left falsy");
    shouldBe(testOuterAnd(0, 1, 0), "F", "outerAnd right falsy");

    shouldBe(testOuterOr(0, 0, 1), "T", "outerOr right truthy");
    shouldBe(testOuterOr(0, 1, 0), "T", "outerOr left truthy");
    shouldBe(testOuterOr(0, 0, 0), "F", "outerOr both falsy");

    shouldBe(testNullish(99, null, 1), "T", "nullish: (a,null) ?? 1");
    shouldBe(testNullish(99, undefined, 0), "F", "nullish: (a,undef) ?? 0");
    shouldBe(testNullish(99, 0, 1), "F", "nullish: (a,0) non-nullish & falsy");
    shouldBe(testNullish(99, 5, 0), "T", "nullish: (a,5) non-nullish & truthy");
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
    if ((s(a, "a"), s(b, "b")))
        return "T";
    return "F";
}
noInline(testOrder);

function testOrderMany(a, b, c) {
    if ((s(a, "a"), s(b, "b"), s(c, "c")))
        return "T";
    return "F";
}
noInline(testOrderMany);

function testOrderInAnd(a, b, c) {
    if ((s(a, "a"), s(b, "b") && s(c, "c")))
        return "T";
    return "F";
}
noInline(testOrderInAnd);

for (let i = 0; i < 1e3; i++) {
    log = [];
    shouldBe(testOrder(0, 1), "T", "order");
    shouldBe(log.join(","), "a,b", "order: both side-effects in order");

    log = [];
    shouldBe(testOrderMany(0, 1, 2), "T", "orderMany");
    shouldBe(log.join(","), "a,b,c", "orderMany: all in order");

    log = [];
    shouldBe(testOrderMany(1, 2, 0), "F", "orderMany falsy last");
    shouldBe(log.join(","), "a,b,c", "orderMany: still all evaluated");

    log = [];
    shouldBe(testOrderInAnd(0, 1, 1), "T", "orderInAnd truthy");
    shouldBe(log.join(","), "a,b,c", "orderInAnd: a then b then c");

    log = [];
    shouldBe(testOrderInAnd(0, 0, 1), "F", "orderInAnd: b falsy short-circuit");
    shouldBe(log.join(","), "a,b", "orderInAnd: c skipped on short-circuit");
}

// =========================================================================
// Throws
// =========================================================================

function testThrowFirst() {
    if ((() => { throw new Error("first"); })(), 1)
        return "T";
    return "F";
}
function testThrowLast() {
    if (1, (() => { throw new Error("last"); })())
        return "T";
    return "F";
}
noInline(testThrowFirst);
noInline(testThrowLast);

for (let i = 0; i < 1e3; i++) {
    shouldThrow(() => testThrowFirst(), Error, "throw in first");
    shouldThrow(() => testThrowLast(), Error, "throw in last");
}

// =========================================================================
// Generator / async
// =========================================================================

function* gen(a, b) {
    if ((a, b))
        yield "T";
    else
        yield "F";
}

async function asyncFn(a, b) {
    if ((a, b))
        return "T";
    return "F";
}

(async () => {
    for (let i = 0; i < 100; i++) {
        const it = gen(0, 1);
        shouldBe(it.next().value, "T", "generator truthy");
        const it2 = gen(1, 0);
        shouldBe(it2.next().value, "F", "generator falsy");

        shouldBe(await asyncFn(0, 1), "T", "async truthy");
        shouldBe(await asyncFn(1, 0), "F", "async falsy");
    }
})();

// =========================================================================
// Switch
// =========================================================================

function testSwitch(a, b) {
    switch (true) {
        case (a, b > 5):
            return "big";
        case (a, b > 0):
            return "small";
        default:
            return "none";
    }
}
noInline(testSwitch);

for (let i = 0; i < 1e3; i++) {
    shouldBe(testSwitch(99, 10), "big");
    shouldBe(testSwitch(99, 3), "small");
    shouldBe(testSwitch(99, -1), "none");
    shouldBe(testSwitch(99, 0), "none");
}

