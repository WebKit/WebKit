
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

for (var i = 0; i < testLoopCount; ++i)
    new B();
