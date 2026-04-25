class A extends Object {
}

class B extends A {
    constructor() {
        super();
    }
}

function test()
{
    return new B();
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    test();
}
