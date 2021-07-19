function assert(b) {
    if (!b)
        throw new Error;
}

function doTest(o, m) {
    let error;
    try {
        o[m]();
    } catch(e) {
        error = e;
    }

    assert(!!error);
    assert(error instanceof SyntaxError);
    assert(error.message.startsWith("Cannot reference undeclared private names"));
}

class C {
    #y;
    #method2() { }
    constructor() { }
    a() { eval('this.#x;'); }
    b() { eval('this.#method();'); }
}

for (let i = 0; i < 1000; ++i) { 
    let c = new C();
    doTest(c, "a");
    doTest(c, "b");
}

class D {
    #y;
    #method2() { }
    constructor() { }
    a() {
        class C {
            #y2;
            #method3() { }
            a() { 
                eval('this.#x;');
            }
        }

        let x = new C;
        x.a();
    }

    b() {
        class C {
            #y2;
            #method3() { }
            a() { 
                eval('this.#method();'); 
            }
        }

        let x = new C;
        x.a();
    }
}

for (let i = 0; i < 1000; ++i) {
    let d = new D();
    doTest(d, "a");
    doTest(d, "b");
}
