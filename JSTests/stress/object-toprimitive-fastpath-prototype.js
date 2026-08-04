function shouldBe(a, b) { if (a !== b) throw new Error("bad: " + a + " !== " + b); }

// A special property on the [[Prototype]] (not own) must still be honored: the fast path only
// checks own properties for memoization, but the eligibility itself resolves valueOf / toString /
// @@toPrimitive up the whole prototype chain, so it must never fold such objects to "[object Object]".

// Prototype has valueOf: number/default hints use it; string hint falls to the primordial toString.
(function () {
    function O() {}
    O.prototype = { valueOf() { return 111; } };
    var o = new O();
    for (var i = 0; i < testLoopCount; ++i) { o < 1; o + ""; `${o}`; }
    shouldBe(o < 200, true);
    shouldBe(200 < o, false);
    shouldBe(o + 1, 112);
    shouldBe(`${o}`, "[object Object]");
})();

// Prototype has toString.
(function () {
    function O() {}
    O.prototype = { toString() { return "PROTO"; } };
    var o = new O();
    for (var i = 0; i < testLoopCount; ++i) { `${o}`; o < 1; o + ""; }
    shouldBe(`${o}`, "PROTO");
    shouldBe(o + "", "PROTO");
    shouldBe(String(o), "PROTO");
})();

// Prototype has Symbol.toPrimitive.
(function () {
    function O() {}
    O.prototype = { [Symbol.toPrimitive](hint) { return hint === "string" ? "S" : 7; } };
    var o = new O();
    for (var i = 0; i < testLoopCount; ++i) { o < 1; `${o}`; o + ""; }
    shouldBe(`${o}`, "S");
    shouldBe(o < 10, true);
    shouldBe(o + 1, 8);
})();

// Prototype has a string Symbol.toStringTag: coercion reflects it.
(function () {
    function O() {}
    O.prototype = {};
    O.prototype[Symbol.toStringTag] = "PT";
    var o = new O();
    for (var i = 0; i < testLoopCount; ++i) { `${o}`; o < 1; }
    shouldBe(`${o}`, "[object PT]");
    shouldBe(Object.prototype.toString.call(o), "[object PT]");
})();

// Reach the fast path as a plain object, then insert a prototype with valueOf: must deopt.
(function () {
    var o = {};
    for (var i = 0; i < testLoopCount; ++i) { o < 1; `${o}`; o + ""; }
    shouldBe(o + 1, "[object Object]1");

    Object.setPrototypeOf(o, { valueOf() { return 5; } });
    shouldBe(o < 10, true);
    shouldBe(o + 1, 6);
    shouldBe(`${o}`, "[object Object]");
})();

// Reach the fast path, then insert a prototype with valueOf into the middle of the chain.
(function () {
    function O() {}
    var o = new O();
    for (var i = 0; i < testLoopCount; ++i) { o < 1; `${o}`; o + ""; }
    shouldBe(o + 1, "[object Object]1");

    Object.setPrototypeOf(O.prototype, { valueOf() { return 9; } });
    shouldBe(o < 10, true);
    shouldBe(o + 1, 10);
})();

// Reach the fast path, then add an own valueOf directly.
(function () {
    var o = {};
    for (var i = 0; i < testLoopCount; ++i) { o < 1; `${o}`; o + ""; }
    o.valueOf = function () { return 3; };
    shouldBe(o < 10, true);
    shouldBe(o + 1, 4);
})();
