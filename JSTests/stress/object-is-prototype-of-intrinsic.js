function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad: " + actual + " !== " + expected);
}

function shouldThrow(func, errorType) {
    let thrown = false;
    try {
        func();
    } catch (e) {
        thrown = true;
        if (!(e instanceof errorType))
            throw new Error("bad error type: " + e);
    }
    if (!thrown)
        throw new Error("not thrown");
}

var isPrototypeOf = Object.prototype.isPrototypeOf;

// Basic hit / miss with class hierarchy.
(function() {
    class A { }
    class B extends A { }
    class C extends B { }
    var c = new C();
    var unrelated = {};

    function hit(o) { return A.prototype.isPrototypeOf(o); }
    function miss(o) { return unrelated.isPrototypeOf(o); }
    noInline(hit);
    noInline(miss);

    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(hit(c), true);
        shouldBe(miss(c), false);
    }
})();

// Argument is not an object → false (no exception even with null/undefined receiver).
(function() {
    function test(proto, value) { return isPrototypeOf.call(proto, value); }
    noInline(test);

    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(test(Object.prototype, 42), false);
        shouldBe(test(Object.prototype, "foo"), false);
        shouldBe(test(Object.prototype, null), false);
        shouldBe(test(Object.prototype, undefined), false);
        shouldBe(test(Object.prototype, true), false);
        shouldBe(test(Object.prototype, Symbol()), false);
        // V is not an object so step 1 returns false before ToObject(this).
        shouldBe(test(null, 42), false);
        shouldBe(test(undefined, "foo"), false);
    }
})();

// Receiver is null/undefined and argument is an object → TypeError.
(function() {
    function test(proto, value) { return isPrototypeOf.call(proto, value); }
    noInline(test);
    var obj = {};

    for (var i = 0; i < testLoopCount; ++i) {
        // Warm up with an object receiver so the intrinsic compiles, then OSR exit.
        shouldBe(test(Object.prototype, obj), true);
    }
    shouldThrow(() => test(null, obj), TypeError);
    shouldThrow(() => test(undefined, obj), TypeError);
})();

// Receiver is a primitive (not null/undefined) → always false because the wrapper
// object is freshly allocated and cannot appear in any prototype chain.
(function() {
    function test(proto, value) { return isPrototypeOf.call(proto, value); }
    noInline(test);
    var obj = Object.create(String.prototype);

    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(test(String.prototype, obj), true);
    shouldBe(test("foo", obj), false);
    shouldBe(test(42, Object.create(Number.prototype)), false);
})();

// Polymorphic value with hit/miss alternation.
(function() {
    class Foo { }
    class Bar { }
    var foo = new Foo();
    var bar = new Bar();
    var proto = Foo.prototype;

    function test(o) { return proto.isPrototypeOf(o); }
    noInline(test);

    for (var i = 0; i < testLoopCount; ++i) {
        var o = (i & 1) ? foo : bar;
        shouldBe(test(o), !!(i & 1));
    }
})();

// Proxy in the chain: ensure [[GetPrototypeOf]] trap is called.
(function() {
    var trapCalls = 0;
    var target = {};
    var proxy = new Proxy(target, {
        getPrototypeOf() {
            trapCalls++;
            return Object.prototype;
        }
    });
    var leaf = Object.create(proxy);

    function test(o) { return Object.prototype.isPrototypeOf(o); }
    noInline(test);

    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(test(leaf), true);
    shouldBe(trapCalls, testLoopCount);
})();
