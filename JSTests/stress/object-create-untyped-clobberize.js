function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldThrow(func, errorType) {
    var error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error("bad error: " + error);
}

(function testCSEAcrossUntypedCreate() {
    function go(o, proto) {
        var a = o.value;
        Object.create(proto);
        var b = o.value;
        return a + b;
    }
    noInline(go);

    var o = { value: 21 };
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(go(o, i & 1 ? {} : null), 42);
})();

(function testThrowOnBadPrototype() {
    function go(o, proto) {
        var a = o.value;
        Object.create(proto);
        return a + o.value;
    }
    noInline(go);

    var o = { value: 1 };
    for (var i = 0; i < testLoopCount; ++i)
        go(o, null);
    shouldThrow(() => go(o, 42), TypeError);
    shouldThrow(() => go(o, "string"), TypeError);
    shouldThrow(() => go(o, undefined), TypeError);
})();

(function testNoHoistPastGuard() {
    function go(taken, proto) {
        var result = 0;
        for (var i = 0; i < 100; ++i) {
            if (taken)
                result += Object.create(proto) !== null;
        }
        return result;
    }
    noInline(go);

    for (var i = 0; i < testLoopCount; ++i)
        go(true, null);
    shouldBe(go(false, 42), 0);
})();

(function testResultPrototype() {
    function go(proto) {
        return Object.create(proto);
    }
    noInline(go);

    var p = { tag: 1 };
    for (var i = 0; i < testLoopCount; ++i) {
        if (i & 1)
            shouldBe(Object.getPrototypeOf(go(null)), null);
        else
            shouldBe(Object.getPrototypeOf(go(p)), p);
    }
})();
