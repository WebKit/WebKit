
class A {
    constructor() { }
}

class B extends A {
    constructor() {
        eval("this");
        super();
    }
}

noInline(B);

for (var i = 0; i < 1e4; ++i) {
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
