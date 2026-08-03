function shouldBe(a, b) { if (a !== b) throw new Error("bad: " + a + " !== " + b); }

// Objects with an own or exotic override must always take the full, spec-compliant slow path.

// own valueOf returning a number.
(function () {
    var v = { valueOf() { return 5; } };
    shouldBe(v < 10, true);
    shouldBe(v + 1, 6);
})();

// own toString.
(function () {
    var s = { toString() { return "z"; } };
    shouldBe(`${s}`, "z");
})();

// own Symbol.toPrimitive.
(function () {
    var tp = { [Symbol.toPrimitive](hint) { return hint; } };
    shouldBe(`${tp}`, "string");
    shouldBe(Number.isNaN(+tp), true);
    shouldBe(tp + "", "default");
})();

// own Symbol.toStringTag.
(function () {
    var tag = { [Symbol.toStringTag]: "X" };
    shouldBe(`${tag}`, "[object X]");
})();

// Mixed-operand relational compares against a plain object.
(function () {
    var o = {};
    shouldBe(o < 5, false);
    shouldBe(1 < o, false);
    shouldBe("a" < o, false);
    shouldBe(o < "b", true);
    shouldBe(o <= 5, false);
    shouldBe(5 <= o, false);
    shouldBe(o > 5, false);
    shouldBe(o >= 5, false);
})();

// Arrays use their own fast path, not the plain-object one.
(function () {
    shouldBe([1, 2] < [1, 3], true);
})();

// Object.create(null) has no toString/valueOf/toPrimitive, so ToPrimitive throws.
(function () {
    var n = Object.create(null);
    var threw = false;
    try {
        `${n}`;
    } catch (e) {
        threw = e instanceof TypeError;
    }
    shouldBe(threw, true);

    threw = false;
    try {
        n + 1;
    } catch (e) {
        threw = e instanceof TypeError;
    }
    shouldBe(threw, true);

    threw = false;
    try {
        n < 5;
    } catch (e) {
        threw = e instanceof TypeError;
    }
    shouldBe(threw, true);
})();

// Object identity (a == b) is unaffected, same-realm and cross-realm.
(function () {
    var a = {}, b = {};
    shouldBe(a == b, false);
    shouldBe(a == a, true);

    var other = createGlobalObject();
    var otherObjectCtor = other.Object;
    var crossA = new otherObjectCtor();
    var crossB = new otherObjectCtor();
    shouldBe(crossA == crossB, false);
    shouldBe(crossA == crossA, true);
})();

// Cross-realm: the verdict must be computed against the object's own realm, so an override on
// the other realm's Object.prototype.toString is honored when coercing from the main realm.
(function () {
    var other = createGlobalObject();
    var o = new other.Object();

    var p = new other.Object();
    for (var i = 0; i < testLoopCount; ++i) {
        o < p;
        o <= p;
        `${o}`;
    }

    shouldBe(`${o}`, "[object Object]");
    shouldBe(o + "", "[object Object]");
    shouldBe(String(o), "[object Object]");
    shouldBe(o < o, false);
    shouldBe(o <= o, true);
    shouldBe(o > o, false);
    shouldBe(o >= o, true);

    other.Object.prototype.toString = function () { return "custom-other-realm"; };
    shouldBe(`${o}`, "custom-other-realm");
    shouldBe(o + "", "custom-other-realm");
    shouldBe(String(o), "custom-other-realm");
})();

// The default fast path coexists with all the overridden cases above without cross-contamination.
(function () {
    function Plain() {}
    var p = new Plain();
    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(`${p}`, "[object Object]");
        shouldBe(p < 5, false);
    }
})();
