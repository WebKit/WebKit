
class A {
    constructor() { }
}

class B extends A {
    constructor() {
        try {
            this;
        } catch (e) {
            super();
        }
    }
}

noInline(B);

for (var i = 0; i < 10000; ++i)
    new B();
