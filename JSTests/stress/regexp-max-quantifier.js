function assert(a) {
    if (!a)
        throw new Error();
}

function shouldThrow(expectedError, func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("Not thrown");
    if (!(error instanceof expectedError))
        throw new Error(`Expected "${expectedError.name}" but got "${error.name}"`);
    if (error.message !== message)
        throw new Error(`Expected message "${message}" but got "${error.message}"`);
}

{
    // Number.MAX_SAFE_INTEGER
    var re1 = new RegExp("b{9007199254740991}", "u");
    assert(!re1.test(""));

    var re2 = new RegExp("b{9007199254740991,}?");
    assert(!re2.test("a"));

    var re3 = new RegExp("b{9007199254740991,9007199254740991}");
    assert(!re3.test("b"));
}

{
    // Number.MAX_SAFE_INTEGER + 1
    var re1 = new RegExp("b{9007199254740992}", "u");
    assert(!re1.test(""));

    var re2 = new RegExp("b{9007199254740992,}?");
    assert(!re2.test("a"));

    var re3 = new RegExp("b{9007199254740992,9007199254740992}");
    assert(!re3.test("b"));
}

{
    // UINT64_MAX - 1
    var re1 = new RegExp("b{18446744073709551614}", "u");
    assert(!re1.test(""));

    var re2 = new RegExp("b{18446744073709551614,}?");
    assert(!re2.test("a"));

    var re3 = new RegExp("b{18446744073709551614,18446744073709551614}");
    assert(!re3.test("b"));
}

{
    // UINT64_MAX
    shouldThrow(SyntaxError, () => {
        eval(`new RegExp("b{18446744073709551615}", "u")`);
    }, "Invalid regular expression: number too large in {} quantifier");

    shouldThrow(SyntaxError, () => {
        eval(`new RegExp("b{18446744073709551615,}?")`);
    }, "Invalid regular expression: number too large in {} quantifier");

    shouldThrow(SyntaxError, () => {
        eval(`new RegExp("b{18446744073709551615,18446744073709551615}")`);
    }, "Invalid regular expression: number too large in {} quantifier");
}

{
    // UINT64_MAX + 1
    shouldThrow(SyntaxError, () => {
        eval(`new RegExp("b{18446744073709551616}", "u")`);
    }, "Invalid regular expression: number too large in {} quantifier");

    shouldThrow(SyntaxError, () => {
        eval(`new RegExp("b{18446744073709551616,}?")`);
    }, "Invalid regular expression: number too large in {} quantifier");

    shouldThrow(SyntaxError, () => {
        eval(`new RegExp("b{18446744073709551616,18446744073709551616}")`);
    }, "Invalid regular expression: number too large in {} quantifier");
}
