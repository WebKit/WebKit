
class A {
    constructor() { }
}

class B extends A {
    constructor() {
        for (var j = 0; j < 10; j++) {
            if (!j)
                super();
            else
                this;
        }
    }
}

noInline(B);

for (var i = 0; i < testLoopCount; ++i)
    new B();
