var testValue  = 'test-value';

class A {
    constructor() {
        this.value = testValue;
    }
}

class B extends A {
    constructor() {
        super();
        var arrow  = () => testValue;
        arrow();
    }
}

noInline(B);

for (let i = 0; i < 1000000; ++i) {
    let b = new B();
    if (b.value != testValue)
        throw "Error: bad result: " + result;
}
