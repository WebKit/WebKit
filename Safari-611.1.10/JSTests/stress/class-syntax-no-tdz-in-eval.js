
class A {
    constructor() { }
}

class B extends A {
    constructor(shouldAccessThis) {
        var evalFunction = eval;
        evalFunction("this");
        eval("shouldAccessThis ? this : null");
        super();
    }
}

noInline(B);

for (var i = 0; i < 1e4; ++i)
    new B(false);
