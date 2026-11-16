function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Expected: " + expected + ", actual: " + actual);
}

function testBasic(a, b, c) {
    if (a ? b : c)
        return 1;
    return 0;
}

function testConstantThen(a) {
    if (a ? true : false)
        return 1;
    return 0;
}

function testConstantElse(a) {
    if (a ? false : true)
        return 1;
    return 0;
}

function testLogicalNot(a, b, c) {
    if (a ? !b : c)
        return 1;
    return 0;
}

function testLogicalAnd(a, b, c, d, e) {
    if (a ? b && c : d || e)
        return 1;
    return 0;
}

function testNested(a, b, c, d, e) {
    if (a ? (b ? c : d) : e)
        return 1;
    return 0;
}

function testWhile(a, b, c) {
    var count = 0;
    while (a ? b : c) {
        count++;
        a = false;
        b = false;
        c = false;
    }
    return count;
}

function testFor(a, b, c) {
    var count = 0;
    for (; a ? b : c;) {
        count++;
        a = false;
        b = false;
        c = false;
    }
    return count;
}

function testLogicalOperand(a, b, c, d) {
    if ((a ? b : c) && d)
        return 1;
    return 0;
}

function testLogicalOr(a, b, c, d) {
    if ((a ? b : c) || d)
        return 1;
    return 0;
}

function testDoWhile(a, b, c) {
    var count = 0;
    do {
        count++;
        if (count > 1) {
            a = false;
            b = false;
            c = false;
        }
    } while (a ? b : c);
    return count;
}

for (var i = 0; i < 1e4; i++) {
    shouldBe(testBasic(true, true, false), 1);
    shouldBe(testBasic(true, false, true), 0);
    shouldBe(testBasic(false, true, true), 1);
    shouldBe(testBasic(false, true, false), 0);
    shouldBe(testBasic(false, false, false), 0);
    shouldBe(testBasic(true, true, true), 1);

    shouldBe(testConstantThen(true), 1);
    shouldBe(testConstantThen(false), 0);

    shouldBe(testConstantElse(true), 0);
    shouldBe(testConstantElse(false), 1);

    shouldBe(testLogicalNot(true, true, false), 0);
    shouldBe(testLogicalNot(true, false, true), 1);
    shouldBe(testLogicalNot(false, true, true), 1);
    shouldBe(testLogicalNot(false, true, false), 0);

    shouldBe(testLogicalAnd(true, true, true, false, false), 1);
    shouldBe(testLogicalAnd(true, true, false, false, false), 0);
    shouldBe(testLogicalAnd(true, false, true, false, false), 0);
    shouldBe(testLogicalAnd(false, false, false, true, false), 1);
    shouldBe(testLogicalAnd(false, false, false, false, true), 1);
    shouldBe(testLogicalAnd(false, false, false, false, false), 0);

    shouldBe(testNested(true, true, true, false, false), 1);
    shouldBe(testNested(true, true, false, true, false), 0);
    shouldBe(testNested(true, false, true, true, false), 1);
    shouldBe(testNested(true, false, false, false, true), 0);
    shouldBe(testNested(false, true, true, true, true), 1);
    shouldBe(testNested(false, true, true, true, false), 0);

    shouldBe(testWhile(true, true, false), 1);
    shouldBe(testWhile(false, false, true), 1);
    shouldBe(testWhile(true, false, true), 0);
    shouldBe(testWhile(false, false, false), 0);

    shouldBe(testFor(true, true, false), 1);
    shouldBe(testFor(false, false, true), 1);
    shouldBe(testFor(true, false, true), 0);
    shouldBe(testFor(false, false, false), 0);

    shouldBe(testLogicalOperand(true, true, false, true), 1);
    shouldBe(testLogicalOperand(true, true, false, false), 0);
    shouldBe(testLogicalOperand(true, false, true, true), 0);
    shouldBe(testLogicalOperand(false, false, true, true), 1);
    shouldBe(testLogicalOperand(false, false, false, true), 0);

    shouldBe(testLogicalOr(true, true, false, false), 1);
    shouldBe(testLogicalOr(true, false, true, false), 0);
    shouldBe(testLogicalOr(true, false, true, true), 1);
    shouldBe(testLogicalOr(false, false, true, false), 1);
    shouldBe(testLogicalOr(false, false, false, true), 1);
    shouldBe(testLogicalOr(false, false, false, false), 0);

    shouldBe(testDoWhile(true, true, false), 2);
    shouldBe(testDoWhile(false, false, false), 1);
}
