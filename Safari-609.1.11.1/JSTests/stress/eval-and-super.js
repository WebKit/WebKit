function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

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



class Hello {
    m()
    {
        return eval("super()");
    }

    n()
    {
        return (0, eval)("super()");
    }
};

var hello = new Hello();
shouldThrow(() => hello.m(), `SyntaxError: super is not valid in this context.`);
shouldThrow(() => hello.n(), `SyntaxError: super is not valid in this context.`);
