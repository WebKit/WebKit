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

class A {
    constructor(cond)
    {
        if (!cond)
            throw new Error("OK");
    }
}

class B extends A {}
noInline(B);
for (var i = 0; i < 1e6; ++i)
    new B(1);
shouldThrow(() => new B(0), `Error: OK`);
