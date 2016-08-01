function assert(b, m = "Bad!") {
    if (!b) {
        throw new Error(m);
    }
}

function test(f, iters = 1000) {
    for (let i = 0; i < iters; i++)
        f();
}

function shouldThrowTDZ(f) {
    let threw = false;
    try {
        f();
    } catch(e) {
        assert(e instanceof ReferenceError);
        assert(e.toString() === "ReferenceError: Cannot access uninitialized variable.");
        threw = true;
    }
    assert(threw);
}

test(function() {
    class A {
        get foo() {
            return this._x;
        }
        set foo(x) {
            this._x = x;
        }
    }

    function fooProp() { return 'foo'; }

    class B extends A {
        constructor() {
            super.foo = 20;
        }
    }

    class C extends A {
        constructor() {
            super[fooProp()] = 20;
        }
    }

    class D extends A {
        constructor() {
            super[fooProp()];
        }
    }

    class E extends A {
        constructor() {
            super.foo;
        }
    }

    class F extends A {
        constructor() {
            (() => super.foo = 20)();
        }
    }

    class G extends A {
        constructor() {
            (() => super[fooProp()] = 20)();
        }
    }

    class H extends A {
        constructor() {
            (() => super[fooProp()])();
        }
    }

    class I extends A {
        constructor() {
            (() => super.foo)();
        }
    }

    shouldThrowTDZ(function() { new B; });
    shouldThrowTDZ(function() { new C; });
    shouldThrowTDZ(function() { new D; });
    shouldThrowTDZ(function() { new E; });
    shouldThrowTDZ(function() { new F; });
    shouldThrowTDZ(function() { new G; });
    shouldThrowTDZ(function() { new H; });
    shouldThrowTDZ(function() { new I; });
});
