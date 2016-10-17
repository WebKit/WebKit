function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
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

(function () {
    let domjit = createDOMJITGetterComplexObject();
    function access(object)
    {
        return object.customGetter;
    }
    noInline(access);
    let normal = {
        customGetter: 42
    };
    for (let i = 0; i < 1e4; ++i) {
        shouldBe(access(domjit), 42);
        shouldBe(access(normal), 42);
    }
    domjit.enableException();
    shouldThrow(() => access(domjit), `Error: DOMJITGetterComplex slow call exception`);
}());
