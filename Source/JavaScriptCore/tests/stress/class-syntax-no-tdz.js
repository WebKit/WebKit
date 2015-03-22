
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

for (var i = 0; i < 10000; ++i)
    new B();
