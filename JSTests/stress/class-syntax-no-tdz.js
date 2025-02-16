
class A {
    constructor() { }
}

class B extends A {
    constructor() {
        super();
        this;
    }
}

noInline(B);

for (var i = 0; i < testLoopCount; ++i)
    new B();
