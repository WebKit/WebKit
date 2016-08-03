function assert(b) {
    if (!b)
        throw new Error("Bad!")
}

function test(f, count = 1000) {
    for (let i = 0; i < count; i++)
        f();
}

function test1() {
    class C extends null { }
    assert((new C) instanceof C);
    assert(!((new C) instanceof Object));
    assert(Reflect.getPrototypeOf(C.prototype) === null);

    let o = {}
    class D extends null {
        constructor() {
            return o;
        }
    }
    assert(new D === o);
    assert(Reflect.getPrototypeOf(D.prototype) === null);

    class E extends null {
        constructor() {
            return this;
        }
    }
    assert((new E) instanceof E);
    assert(!((new E) instanceof Object));
    assert(Reflect.getPrototypeOf(E.prototype) === null);
}
test(test1);

function jsNull() { return null; }
function test2() {
    class C extends jsNull() { }
    assert((new C) instanceof C);
    assert(!((new C) instanceof Object));
    assert(Reflect.getPrototypeOf(C.prototype) === null);

    let o = {}
    class D extends jsNull() {
        constructor() {
            return o;
        }
    }
    assert(new D === o);
    assert(Reflect.getPrototypeOf(D.prototype) === null);

    class E extends jsNull() {
        constructor() {
            return this;
        }
    }
    assert((new E) instanceof E);
    assert(!((new E) instanceof Object));
    assert(Reflect.getPrototypeOf(E.prototype) === null);
}
test(test2);

function test3() {
    class C extends jsNull() { constructor() { super(); } }
    let threw = false;
    try {
        new C;
    } catch(e) {
        threw = e.toString() === "TypeError: function is not a constructor (evaluating 'super()')";
    }
    assert(threw);

    class D extends jsNull() { constructor() { let arr = ()=>super(); arr(); } }
    threw = false;
    try {
        new D;
    } catch(e) {
        threw = e.toString() === "TypeError: function is not a constructor (evaluating 'super()')";
    }
    assert(threw);

    class E extends jsNull() { constructor() { let arr = ()=>super(); return this; } }
    assert((new E) instanceof E);
    assert(!((new E) instanceof Object));
    assert(Reflect.getPrototypeOf(E.prototype) === null);
}
test(test3);

function test4() {
    class E extends jsNull() { constructor() { return 25; } }
    assert((new E) instanceof E);
    assert(!((new E) instanceof Object));
    assert(Reflect.getPrototypeOf(E.prototype) === null);
}
test(test4);

function test5() {
    class E extends jsNull() { constructor() { let arr = ()=>this; return arr(); } }
    assert((new E) instanceof E);
    assert(!((new E) instanceof Object));
    assert(Reflect.getPrototypeOf(E.prototype) === null);
}
test(test5);

function test6() {
    class Base { }
    class D extends Base { }
    class E extends jsNull() { constructor() { let ret = this; return ret; } }
    class F extends jsNull() { constructor() { return 25; } }
    class G extends jsNull() { constructor() { super(); } }
    let result = Reflect.construct(E, [], D);
    assert(result instanceof D);
    assert(result instanceof Object);

    result = Reflect.construct(F, [], D);
    assert(result instanceof D);
    assert(result instanceof Object);

    let threw = false;
    try {
        Reflect.construct(G, [], D);
    } catch(e) {
        threw = e.toString() === "TypeError: function is not a constructor (evaluating 'super()')";
    }
    assert(threw);
}
test(test6);
