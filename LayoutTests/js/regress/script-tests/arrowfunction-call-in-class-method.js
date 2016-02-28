var testValue  = 'test-value';

class A {
    constructor() {
        this.value = testValue;
    }

    getValue () {
        return this.value;
    }
}

class B extends A {
    getParentValue() {
        var arrow  = () => testValue;
        return arrow();
    }
}

for (let i = 0; i < 100000; ++i) {
    let b = new B();
    if (b.getParentValue() != testValue)
        throw "Error: bad result: " + result;
}
