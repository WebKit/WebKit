function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function v2() {
    const v8 = RegExp("(1{2147480000,}2{3648,})|(ab)");
    "(1{2147480000,}2{3648,})|(ab)";
    const v4 = RegExp(v2);
    const v5 = v4.test();
    return v5;
}
shouldThrow(v2, `SyntaxError: Invalid regular expression: pattern exceeds string length limits`);
