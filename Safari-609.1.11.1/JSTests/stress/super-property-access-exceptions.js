function assert(b, m = "Bad!") {
    if (!b) {
        throw new Error(m);
    }
}

function test(f, iters = 1000) {
    for (let i = 0; i < iters; i++)
        f();
}

test(function() {
    function fooProp() { return 'foo'; }
    noInline(fooProp);

    let shouldThrow = false;
    class A {
        get foo() {
            if (shouldThrow)
                throw new Error;
            return 20;
        }
        get x() { return this._x; }
    }

    class B extends A {
        constructor(x) {
            super();
            this._x = x;
        }

        bar() {
            this._x = super.foo;
        }

        baz() {
            this._x = super[fooProp()];
        }
    }

    function foo(i) { 
        let b = new B(i);
        noInline(b.__lookupGetter__('foo'));
        let threw = false;
        try {
            b.bar();    
        } catch(e) {
            threw = true;
        }
        if (threw)
            assert(b.x === i);
        else
            assert(b.x === 20);
    }
    function bar(i) { 
        let b = new B(i);
        noInline(b.__lookupGetter__('foo'));
        let threw = false;
        try {
            b.baz();    
        } catch(e) {
            threw = true;
        }
        if (threw)
            assert(b.x === i);
        else
            assert(b.x === 20, "b.x " + b.x + "  " + i);
    }
    noInline(bar);

    for (let i = 0; i < 10000; i++) {
        foo(i);
        bar(i);
    }
    shouldThrow = true;
    foo(23);
    bar(24);

}, 1);

test(function() {
    function fooProp() { return 'foo'; }
    noInline(fooProp);

    function func(i) {
        if (shouldThrow)
            throw new Error();
        return i;
    }
    noInline(func);

    let shouldThrow = false;
    class A {
        set foo(x) {
            this._x = x;
        }
        get x() { return this._x; }
    }

    class B extends A {
        constructor(x) {
            super();
            this._x = x;
        }

        bar(x) {
            super.foo = func(x);
        }

        baz(x) {
            super[fooProp()] = func(x);
        }
    }

    function foo(i) { 
        let b = new B(i);
        noInline(b.__lookupGetter__('foo'));
        let threw = false;
        try {
            b.bar(i + 1);
        } catch(e) {
            threw = true;
        }
        if (threw)
            assert(b.x === i);
        else
            assert(b.x === i + 1);
    }
    function bar(i) { 
        let b = new B(i);
        noInline(b.__lookupGetter__('foo'));
        let threw = false;
        try {
            b.baz(i + 1);
        } catch(e) {
            threw = true;
        }
        if (threw)
            assert(b.x === i);
        else
            assert(b.x === i + 1);
    }
    noInline(bar);

    for (let i = 0; i < 10000; i++) {
        foo(i);
        bar(i);
    }
    shouldThrow = true;
    foo(23);
    bar(24);

}, 1);
