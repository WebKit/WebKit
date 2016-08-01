
class A {
    constructor() { }
}

class B extends A {
    constructor() {
        try {
            this;
        } catch (e) {
            this;
            super();
        }
    }
}

noInline(B);

for (var i = 0; i < 10000; ++i) {
    var exception = null;
    try {
         new B(false);
    } catch (e) {
        exception = e;
        if (!(e instanceof ReferenceError))
            throw "Exception thrown in iteration " + i + " was not a reference error";
    }
    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration " + i;
}
