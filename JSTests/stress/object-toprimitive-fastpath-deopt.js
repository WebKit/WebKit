function shouldBe(a, b) { if (a !== b) throw new Error("bad: " + a + " !== " + b); }

// Scenario 1: overriding Object.prototype.toString must deopt all plain objects, then re-engage.
(function () {
    function O() {}
    var a = new O();
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(`${a}`, "[object Object]");
    a < 1;

    var originalToString = Object.prototype.toString;
    // Delete first: a plain reassignment leaves the memoized mode unable to return to Fast.
    delete Object.prototype.toString;
    Object.prototype.toString = function () { return "custom"; };
    try {
        shouldBe(`${a}`, "custom");
        shouldBe(a + "", "custom");
        shouldBe(String(a), "custom");

        var b = new O();
        shouldBe(`${b}`, "custom");
    } finally {
        // Restore as non-enumerable; a plain assignment would leave toString enumerable.
        delete Object.prototype.toString;
        Object.defineProperty(Object.prototype, "toString", {
            value: originalToString,
            writable: true,
            enumerable: false,
            configurable: true,
        });
    }

    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(`${a}`, "[object Object]");
    a < 1;
})();

// Scenario 2: an own Symbol.toPrimitive added after warmup must be honored, then removable.
(function () {
    function O() {}
    var a = new O();
    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(`${a}`, "[object Object]");
        shouldBe(a < 100, false);
    }

    a[Symbol.toPrimitive] = function (hint) {
        if (hint === "number")
            return 42;
        if (hint === "string")
            return "forty-two";
        return "default-forty-two";
    };

    shouldBe(a < 100, true);
    shouldBe(100 < a, false);
    shouldBe(a + 1, "default-forty-two1");
    shouldBe(`${a}`, "forty-two");
    shouldBe(String(a), "forty-two");
    shouldBe(Number(a), 42);

    delete a[Symbol.toPrimitive];
    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(`${a}`, "[object Object]");
        shouldBe(a < 100, false);
    }
})();

// Scenario 3: a prototype Symbol.toStringTag added after warmup must be honored, then removable.
(function () {
    function Tagged() {}
    var obj = new Tagged();
    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(Object.prototype.toString.call(obj), "[object Object]");
        shouldBe(`${obj}`, "[object Object]");
    }
    obj < 1;

    Tagged.prototype[Symbol.toStringTag] = "Tag";

    shouldBe(Object.prototype.toString.call(obj), "[object Tag]");
    shouldBe(`${obj}`, "[object Tag]");
    shouldBe(obj + "", "[object Tag]");
    shouldBe(String(obj), "[object Tag]");

    function Plain() {}
    var plain = new Plain();
    shouldBe(`${plain}`, "[object Object]");

    delete Tagged.prototype[Symbol.toStringTag];
    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(`${obj}`, "[object Object]");
        obj < 1;
    }
})();
