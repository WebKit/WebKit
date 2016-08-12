
class A {
    constructor() { }
}

noInline(A);

class B extends A {
    constructor() {
        var values = [];
        for (var j = 0; j < 100; j++) {
            if (j == 1)
                super();
            else if (j > 2)
                this;
            else
                values.push(i);
        }
    }
}

noInline(B);

for (var i = 0; i < 10000; ++i)
    new B();
