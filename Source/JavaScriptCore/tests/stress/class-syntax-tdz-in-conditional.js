
class A {
    constructor() { }
}

class B extends A {
    constructor(accessThisBeforeSuper) {
        if (accessThisBeforeSuper)
            this;
        else {
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
    }
    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration " + i;
}
