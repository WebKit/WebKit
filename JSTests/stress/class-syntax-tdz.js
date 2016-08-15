
class A {
    constructor() { }
}

class B extends A {
    constructor() {
        this;
        super();
    }
}

noInline(B);

function assert(b) {
    if (!b)
        throw new Error("Assertion failure");
}
noInline(assert);

function shouldThrowTDZ(func) {
    var hasThrown = false;
    try {
        func();
    } catch(e) {
        if (e.name.indexOf("ReferenceError") !== -1)
            hasThrown = true;
    }
    assert(hasThrown);
}
noInline(shouldThrowTDZ);

function truth() { return true; }
noInline(truth);

for (var i = 0; i < 100; ++i) {
    var exception;
    try {
        new B();
    } catch (e) {
        exception = e;
        if (!(e instanceof ReferenceError))
            throw "Exception thrown in iteration " + i + " was not a reference error";
    }
    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration " + i;
}



;(function() {
    function foo() {
        return A;
        class A { }
    }
    function bar() {
        shouldThrowTDZ(function() { return A; });
        class A { }
    }
    function baz() {
        class C { static m() { return "m"; } }
        if (truth()) {
            class B extends C { }
            assert(B.m() === "m");
        }
        class B extends A { }
        class A { }
    }
    function taz() {
        function f(x) { return x; }
        class C extends f(C) { }
    }

    for (var i = 0; i < 100; i++) {
        shouldThrowTDZ(foo);
        bar();
        shouldThrowTDZ(baz);
        shouldThrowTDZ(taz);
    }
})();
