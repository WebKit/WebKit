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

function testIf(a) {
    if (a?.x)
        return "T";
    return "F";
}
noInline(testIf);

function testNested(a) {
    if (a?.b?.c)
        return "T";
    return "F";
}
noInline(testNested);

function testNot(a) {
    if (!a?.x)
        return "T";
    return "F";
}
noInline(testNot);

function testAnd(a, b) {
    if (a?.x && b?.y)
        return "T";
    return "F";
}
noInline(testAnd);

function testOr(a, b) {
    if (a?.x || b?.y)
        return "T";
    return "F";
}
noInline(testOr);

function testTernary(a) {
    return a?.x ? "T" : "F";
}
noInline(testTernary);

function testWhile(n) {
    let c = 0;
    while (n?.next) {
        c++;
        n = n.next;
    }
    return c;
}
noInline(testWhile);

function testDoWhile(a) {
    let c = 0;
    do {
        c++;
    } while (a?.dec && --a.dec > 0);
    return c;
}
noInline(testDoWhile);

function testFor(a) {
    let c = 0;
    for (; a?.x > 0; a.x--)
        c++;
    return c;
}
noInline(testFor);

function testOptCall(a) {
    if (a?.f?.())
        return "T";
    return "F";
}
noInline(testOptCall);

function testBracket(a, k) {
    if (a?.[k])
        return "T";
    return "F";
}
noInline(testBracket);

function testDelete(a) {
    if (delete a?.x)
        return "T";
    return "F";
}
noInline(testDelete);

// =========================================================================
// Edge cases: deep nesting
// =========================================================================

function testDeep5(a) {
    if (a?.b?.c?.d?.e?.f)
        return "T";
    return "F";
}
noInline(testDeep5);

// =========================================================================
// Edge cases: mixed ?. and . (non-optional parts can throw)
// =========================================================================

function testMixed(a) {
    // a?.b.c : if a is null -> short-circuit
    //          if a.b is null -> TypeError (because .c is not optional)
    if (a?.b.c)
        return "T";
    return "F";
}
noInline(testMixed);

// =========================================================================
// Edge cases: primitive boxing
// =========================================================================

function testPrimitive(a) {
    if (a?.length)
        return "T";
    return "F";
}
noInline(testPrimitive);

// =========================================================================
// Edge cases: complex logical expressions
// =========================================================================

function testComplexLogic(a, b, c) {
    if ((a?.x || b?.y) && !c?.z)
        return "T";
    return "F";
}
noInline(testComplexLogic);

function testNestedTernary(a, b) {
    return a?.x ? (b?.y ? "TT" : "TF") : (b?.y ? "FT" : "FF");
}
noInline(testNestedTernary);

function testDoubleNot(a) {
    if (!!a?.x)
        return "T";
    return "F";
}
noInline(testDoubleNot);

// =========================================================================
// Edge cases: side effects
// =========================================================================

let sideEffectLog = [];
function side(v, tag) {
    sideEffectLog.push(tag);
    return v;
}

function testSideEffect(a, b) {
    if (side(a, "a")?.x && side(b, "b")?.y)
        return "T";
    return "F";
}
noInline(testSideEffect);

function testBracketSideEffect(a) {
    let i = 0;
    if (a?.[i++]?.[i++])
        return `T${i}`;
    return `F${i}`;
}
noInline(testBracketSideEffect);

// =========================================================================
// Edge cases: getters
// =========================================================================

let getterCount = 0;
let getterObj = { get x() { getterCount++; return 1; } };
let getterFalsy = { get x() { getterCount++; return 0; } };
let getterThrows = { get x() { getterCount++; throw new Error("getter threw"); } };

function testGetter(a) {
    if (a?.x)
        return "T";
    return "F";
}
noInline(testGetter);

function testGetterInTry(a) {
    try {
        if (a?.x)
            return "T";
        return "F";
    } catch (e) {
        return "E";
    }
}
noInline(testGetterInTry);

// =========================================================================
// Edge cases: Proxy
// =========================================================================

function testProxy(a) {
    if (a?.x)
        return "T";
    return "F";
}
noInline(testProxy);

// =========================================================================
// Edge cases: Symbol property
// =========================================================================

let sym = Symbol("test");
function testSymbol(a) {
    if (a?.[sym])
        return "T";
    return "F";
}
noInline(testSymbol);

// =========================================================================
// Edge cases: comparison in condition (not pure boolean context)
// =========================================================================

function testCompareUndef(a) {
    // a?.x === undefined : uses value path, not condition context
    // but the outer === is in condition context
    if (a?.x === undefined)
        return "T";
    return "F";
}
noInline(testCompareUndef);

function testCompareNull(a) {
    if (a?.x == null)
        return "T";
    return "F";
}
noInline(testCompareNull);

// =========================================================================
// Edge cases: nullish coalescing (should NOT use condition context internally)
// =========================================================================

function testCoalesce(a) {
    if (a?.x ?? true)
        return "T";
    return "F";
}
noInline(testCoalesce);

// =========================================================================
// Edge cases: parenthesized
// =========================================================================

function testParenthesized(a) {
    if (((a?.x)))
        return "T";
    return "F";
}
noInline(testParenthesized);

// =========================================================================
// Edge cases: if-else chains
// =========================================================================

function testIfElseChain(a, b, c) {
    if (a?.x)
        return "A";
    else if (b?.y)
        return "B";
    else if (c?.z)
        return "C";
    return "N";
}
noInline(testIfElseChain);

// =========================================================================
// Edge cases: switch (should NOT use condition context)
// =========================================================================

function testSwitch(a) {
    switch (a?.x) {
    case undefined:
        return "U";
    case 1:
        return "1";
    default:
        return "D";
    }
}
noInline(testSwitch);

// =========================================================================
// Edge cases: polymorphic types (exercise OSR)
// =========================================================================

function testPoly(a) {
    if (a?.x)
        return "T";
    return "F";
}
noInline(testPoly);

// =========================================================================
// Edge cases: inside try-finally
// =========================================================================

function testFinally(a) {
    let r = "N";
    try {
        if (a?.x)
            r = "T";
        else
            r = "F";
    } finally {
        r += "!";
    }
    return r;
}
noInline(testFinally);

// =========================================================================
// Edge cases: inside generator
// =========================================================================

function* testGenerator(a) {
    if (a?.x)
        yield "T";
    else
        yield "F";
}

// =========================================================================
// Edge cases: inside async
// =========================================================================

async function testAsync(a) {
    if (a?.x)
        return "T";
    return "F";
}

// =========================================================================
// Edge cases: this in optional chain
// =========================================================================

function testThis() {
    if (this?.x)
        return "T";
    return "F";
}
noInline(testThis);

// =========================================================================
// Edge cases: nested optional chain in condition of inner optional chain
// =========================================================================

function testInnerChain(a, b) {
    if (a?.[b?.k])
        return "T";
    return "F";
}
noInline(testInnerChain);

// =========================================================================
// Edge cases: comma operator
// =========================================================================

function testComma(a) {
    if ((0, a?.x))
        return "T";
    return "F";
}
noInline(testComma);

// =========================================================================
// Run all tests
// =========================================================================

let proxyTrapCount = 0;
let proxyTruthy = new Proxy({}, {
    get(t, k) { proxyTrapCount++; return 1; },
    has(t, k) { return true; }
});

for (let i = 0; i < testLoopCount; i++) {
    // Basic
    shouldBe(testIf(null), "F", "testIf(null)");
    shouldBe(testIf(undefined), "F", "testIf(undefined)");
    shouldBe(testIf({}), "F", "testIf({})");
    shouldBe(testIf({x: 0}), "F", "testIf({x:0})");
    shouldBe(testIf({x: 1}), "T", "testIf({x:1})");
    shouldBe(testIf({x: ""}), "F", "testIf({x:''})");
    shouldBe(testIf({x: "a"}), "T", "testIf({x:'a'})");
    shouldBe(testIf({x: NaN}), "F", "testIf({x:NaN})");
    shouldBe(testIf({x: false}), "F", "testIf({x:false})");
    shouldBe(testIf({x: []}), "T", "testIf({x:[]})");

    shouldBe(testNested(null), "F", "testNested(null)");
    shouldBe(testNested({}), "F", "testNested({})");
    shouldBe(testNested({b: null}), "F", "testNested({b:null})");
    shouldBe(testNested({b: {}}), "F", "testNested({b:{}})");
    shouldBe(testNested({b: {c: 1}}), "T", "testNested({b:{c:1}})");
    shouldBe(testNested({b: {c: 0}}), "F", "testNested({b:{c:0}})");

    shouldBe(testNot(null), "T", "testNot(null)");
    shouldBe(testNot({x: 1}), "F", "testNot({x:1})");
    shouldBe(testNot({x: 0}), "T", "testNot({x:0})");

    shouldBe(testAnd(null, {y: 1}), "F", "testAnd(null,{y:1})");
    shouldBe(testAnd({x: 1}, null), "F", "testAnd({x:1},null)");
    shouldBe(testAnd({x: 1}, {y: 1}), "T", "testAnd({x:1},{y:1})");
    shouldBe(testAnd({x: 0}, {y: 1}), "F", "testAnd({x:0},{y:1})");

    shouldBe(testOr(null, null), "F", "testOr(null,null)");
    shouldBe(testOr({x: 1}, null), "T", "testOr({x:1},null)");
    shouldBe(testOr(null, {y: 1}), "T", "testOr(null,{y:1})");
    shouldBe(testOr({x: 0}, {y: 0}), "F", "testOr({x:0},{y:0})");

    shouldBe(testTernary(null), "F", "testTernary(null)");
    shouldBe(testTernary({x: 1}), "T", "testTernary({x:1})");
    shouldBe(testTernary({x: 0}), "F", "testTernary({x:0})");

    shouldBe(testWhile(null), 0, "testWhile(null)");
    shouldBe(testWhile({next: {next: {next: null}}}), 2, "testWhile(chain)");

    shouldBe(testDoWhile(null), 1, "testDoWhile(null)");
    shouldBe(testDoWhile({dec: 3}), 3, "testDoWhile({dec:3})");

    shouldBe(testFor(null), 0, "testFor(null)");
    shouldBe(testFor({x: 3}), 3, "testFor({x:3})");

    shouldBe(testOptCall(null), "F", "testOptCall(null)");
    shouldBe(testOptCall({}), "F", "testOptCall({})");
    shouldBe(testOptCall({f: () => 1}), "T", "testOptCall({f:()=>1})");
    shouldBe(testOptCall({f: () => 0}), "F", "testOptCall({f:()=>0})");
    shouldBe(testOptCall({f: null}), "F", "testOptCall({f:null})");

    shouldBe(testBracket(null, "x"), "F", "testBracket(null)");
    shouldBe(testBracket({x: 1}, "x"), "T", "testBracket({x:1})");
    shouldBe(testBracket([1], 0), "T", "testBracket([1],0)");

    shouldBe(testDelete(null), "T", "testDelete(null)");
    shouldBe(testDelete({x: 1}), "T", "testDelete({x:1})");

    // Deep nesting
    shouldBe(testDeep5(null), "F", "testDeep5(null)");
    shouldBe(testDeep5({b: {c: {d: {e: {f: 1}}}}}), "T", "testDeep5(full)");
    shouldBe(testDeep5({b: {c: null}}), "F", "testDeep5(partial)");

    // Mixed ?. and .
    shouldBe(testMixed(null), "F", "testMixed(null)");
    shouldBe(testMixed({b: {c: 1}}), "T", "testMixed({b:{c:1}})");
    shouldThrow(() => testMixed({b: null}), TypeError, "testMixed({b:null}) throws");

    // Primitive boxing
    shouldBe(testPrimitive(null), "F", "testPrimitive(null)");
    shouldBe(testPrimitive(""), "F", "testPrimitive('')");
    shouldBe(testPrimitive("hello"), "T", "testPrimitive('hello')");
    shouldBe(testPrimitive([]), "F", "testPrimitive([])");
    shouldBe(testPrimitive([1, 2]), "T", "testPrimitive([1,2])");

    // Complex logic
    shouldBe(testComplexLogic({x: 1}, null, null), "T", "testComplexLogic(1,_,null)");
    shouldBe(testComplexLogic(null, {y: 1}, null), "T", "testComplexLogic(_,1,null)");
    shouldBe(testComplexLogic({x: 1}, null, {z: 1}), "F", "testComplexLogic(1,_,1)");
    shouldBe(testComplexLogic(null, null, null), "F", "testComplexLogic(_,_,_)");

    shouldBe(testNestedTernary({x: 1}, {y: 1}), "TT", "testNestedTernary(1,1)");
    shouldBe(testNestedTernary({x: 1}, null), "TF", "testNestedTernary(1,null)");
    shouldBe(testNestedTernary(null, {y: 1}), "FT", "testNestedTernary(null,1)");
    shouldBe(testNestedTernary(null, null), "FF", "testNestedTernary(null,null)");

    shouldBe(testDoubleNot(null), "F", "testDoubleNot(null)");
    shouldBe(testDoubleNot({x: 1}), "T", "testDoubleNot({x:1})");
    shouldBe(testDoubleNot({x: 0}), "F", "testDoubleNot({x:0})");

    // Side effects (run once per outer loop to avoid excessive log)
    if (i % 100 === 0) {
        sideEffectLog = [];
        testSideEffect(null, {y: 1});
        shouldBe(sideEffectLog.join(","), "a", "testSideEffect: a null -> only a evaluated");

        sideEffectLog = [];
        testSideEffect({x: 1}, null);
        shouldBe(sideEffectLog.join(","), "a,b", "testSideEffect: a.x truthy -> b evaluated");

        sideEffectLog = [];
        testSideEffect({x: 0}, {y: 1});
        shouldBe(sideEffectLog.join(","), "a", "testSideEffect: a.x falsy -> b not evaluated");
    }

    // Bracket side effects
    shouldBe(testBracketSideEffect(null), "F0", "testBracketSideEffect(null) i=0");
    shouldBe(testBracketSideEffect([[1, 2]]), "T2", "testBracketSideEffect([[1,2]]) i=2");
    shouldBe(testBracketSideEffect([null]), "F1", "testBracketSideEffect([null]) i=1");
    shouldBe(testBracketSideEffect([[1]]), "F2", "testBracketSideEffect([[1]]) i=2");

    // Getters
    getterCount = 0;
    testGetter(getterObj);
    shouldBe(getterCount, 1, "getter called once");
    testGetter(null);
    shouldBe(getterCount, 1, "getter not called on null");
    testGetter(getterFalsy);
    shouldBe(getterCount, 2, "getter falsy called once");

    getterCount = 0;
    shouldBe(testGetterInTry(getterThrows), "E", "testGetterInTry throws");
    shouldBe(getterCount, 1, "throwing getter called once");
    shouldBe(testGetterInTry(null), "F", "testGetterInTry(null)");

    // Proxy
    proxyTrapCount = 0;
    shouldBe(testProxy(proxyTruthy), "T", "testProxy truthy");
    shouldBe(proxyTrapCount, 1, "proxy trap called once");
    shouldBe(testProxy(null), "F", "testProxy(null)");
    shouldBe(proxyTrapCount, 1, "proxy trap not called on null");

    // Symbol
    shouldBe(testSymbol(null), "F", "testSymbol(null)");
    shouldBe(testSymbol({[sym]: 1}), "T", "testSymbol({[sym]:1})");
    shouldBe(testSymbol({}), "F", "testSymbol({})");

    // Comparison (value path)
    shouldBe(testCompareUndef(null), "T", "testCompareUndef(null)");
    shouldBe(testCompareUndef({}), "T", "testCompareUndef({})");
    shouldBe(testCompareUndef({x: 1}), "F", "testCompareUndef({x:1})");
    shouldBe(testCompareUndef({x: undefined}), "T", "testCompareUndef({x:undefined})");

    shouldBe(testCompareNull(null), "T", "testCompareNull(null)");
    shouldBe(testCompareNull({x: null}), "T", "testCompareNull({x:null})");
    shouldBe(testCompareNull({x: 0}), "F", "testCompareNull({x:0})");

    // Nullish coalescing
    shouldBe(testCoalesce(null), "T", "testCoalesce(null)");
    shouldBe(testCoalesce({x: 0}), "F", "testCoalesce({x:0})");
    shouldBe(testCoalesce({x: 1}), "T", "testCoalesce({x:1})");
    shouldBe(testCoalesce({x: false}), "F", "testCoalesce({x:false})");

    // Parenthesized
    shouldBe(testParenthesized(null), "F", "testParenthesized(null)");
    shouldBe(testParenthesized({x: 1}), "T", "testParenthesized({x:1})");

    // If-else chain
    shouldBe(testIfElseChain({x: 1}, null, null), "A", "testIfElseChain A");
    shouldBe(testIfElseChain(null, {y: 1}, null), "B", "testIfElseChain B");
    shouldBe(testIfElseChain(null, null, {z: 1}), "C", "testIfElseChain C");
    shouldBe(testIfElseChain(null, null, null), "N", "testIfElseChain N");

    // Switch (not optimized, uses value path)
    shouldBe(testSwitch(null), "U", "testSwitch(null)");
    shouldBe(testSwitch({x: 1}), "1", "testSwitch({x:1})");
    shouldBe(testSwitch({x: 2}), "D", "testSwitch({x:2})");

    // Polymorphic
    shouldBe(testPoly(null), "F", "testPoly(null)");
    shouldBe(testPoly({x: 1}), "T", "testPoly(obj)");
    shouldBe(testPoly("str"), "F", "testPoly(str)");
    shouldBe(testPoly(42), "F", "testPoly(num)");

    // Finally
    shouldBe(testFinally(null), "F!", "testFinally(null)");
    shouldBe(testFinally({x: 1}), "T!", "testFinally({x:1})");

    // This
    shouldBe(testThis.call(null), "F", "testThis(null)");
    shouldBe(testThis.call({x: 1}), "T", "testThis({x:1})");
    shouldBe(testThis.call(undefined), "F", "testThis(undefined)");

    // Inner chain
    shouldBe(testInnerChain(null, {k: "x"}), "F", "testInnerChain(null, {k})");
    shouldBe(testInnerChain({x: 1}, {k: "x"}), "T", "testInnerChain({x:1}, {k:x})");
    shouldBe(testInnerChain({x: 1}, null), "F", "testInnerChain({x:1}, null)");

    // Comma
    shouldBe(testComma(null), "F", "testComma(null)");
    shouldBe(testComma({x: 1}), "T", "testComma({x:1})");
}

// Generator (run outside hot loop)
shouldBe(testGenerator(null).next().value, "F", "testGenerator(null)");
shouldBe(testGenerator({x: 1}).next().value, "T", "testGenerator({x:1})");

// Async (run outside hot loop)
testAsync(null).then(v => shouldBe(v, "F", "testAsync(null)"));
testAsync({x: 1}).then(v => shouldBe(v, "T", "testAsync({x:1})"));
drainMicrotasks();
