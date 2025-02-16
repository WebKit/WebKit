(function () {
    function shouldBe(actual, expected) {
        if (actual !== expected)
            throw new Error(`bad value: ${actual}`);
    }
    noInline(shouldBe);

    function target() {
        var symbol = Symbol("Cocoa");
        shouldBe(typeof symbol, "symbol");
    }
    noInline(target);

    for (var i = 0; i < testLoopCount; ++i)
        target();
}());
