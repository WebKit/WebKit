function assert(b) {
    if (!b)
        throw new Error("Bad!")
}

function assertThrow(cb, errorMessage) {
    let error;
    try {
        cb();
    } catch (e) {
        error = e;
    }
    if (!error || !(error instanceof Error)) 
        throw new Error("Error is expected!");

    if (error.toString() !== errorMessage) 
        throw new Error("Error: `" + errorMessage + "` is expected, but was `" + error.toString() + "`");
}

function test(f, count = 1000) {
    for (let i = 0; i < count; i++)
        f();
}

function test1() {
    class C extends null { }
    assertThrow(() => (new C), 'TypeError: function is not a constructor (evaluating \'super(...args)\')');
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
    assertThrow(()=>(new E), 'ReferenceError: Cannot access uninitialized variable.');
    assert(Reflect.getPrototypeOf(E.prototype) === null);
}
test(test1);

function jsNull() { return null; }
function test2() {
    class C extends jsNull() { }
    assertThrow(() => (new C), 'TypeError: function is not a constructor (evaluating \'super(...args)\')');
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
    assert(() => (new E), 'ReferenceError: Cannot access uninitialized variable.');
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
    assert(()=>(new E), 'ReferenceError: Cannot access uninitialized variable.');
    assert(Reflect.getPrototypeOf(E.prototype) === null);
}
test(test3);

function test4() {
    class E extends jsNull() { constructor() { return 25; } }
    assert(() => (new E), 'ReferenceError: Cannot access uninitialized variable.');
    assert(Reflect.getPrototypeOf(E.prototype) === null);
}
test(test4);

function test5() {
    class E extends jsNull() { constructor() { let arr = ()=>this; return arr(); } }
    assert(()=>(new E), 'ReferenceError: Cannot access uninitialized variable.');
    assert(Reflect.getPrototypeOf(E.prototype) === null);
}
test(test5);

function test6() {
    class Base { }
    class D extends Base { }
    class E extends jsNull() { constructor() { let ret = this; return ret; } }
    class F extends jsNull() { constructor() { return 25; } }
    class G extends jsNull() { constructor() { super(); } }
    assertThrow(() => Reflect.construct(E, [], D), 'ReferenceError: Cannot access uninitialized variable.');
    assertThrow(() => Reflect.construct(F, [], D), 'TypeError: Cannot return a non-object type in the constructor of a derived class.');

    let threw = false;
    try {
        Reflect.construct(G, [], D);
    } catch(e) {
        threw = e.toString() === "TypeError: function is not a constructor (evaluating 'super()')";
    }
    assert(threw);
}
test(test6);
