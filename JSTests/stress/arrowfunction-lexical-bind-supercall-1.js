var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var testValue  = 'test-value';

var A = class A {
    constructor() {
        this.idValue = testValue;
    }
};

var B = class B extends A {
    constructor (inArrowFuction, inConstructor) {
        var arrow = () => {
            if (inArrowFuction) {
                super();
                testCase(this.idValue, testValue, "Error: super() should create this and put value into idValue property");
            }
        }

        if (inArrowFuction)
            arrow();

        if (inConstructor)
            super();

        testCase(this.idValue, testValue, "Error: arrow function should return this to constructor");
    }
};

for (var i = 0; i < 1000; i++) {
    new B(true, false);
    new B(false, true);
}

var testException = function (value1, value2, index) {
    var exception;
    try {
        new B(value1, value2);
    } catch (e) {
        exception = e;
        if (!(e instanceof ReferenceError))
            throw "Exception thrown was not a reference error";
    }

    if (!exception)
        throw "Exception not thrown for an unitialized this at iteration " + index;
}

for (var i=0; i < 1000; i++) {
    testException(false, false, i);
}

var C = class C extends A {
    constructor() {
        eval("var x = 20");
        super();
        let f = () => this;
        let xf = f();
        xf.id = 'test-id';
    }
};

var c = new C();
