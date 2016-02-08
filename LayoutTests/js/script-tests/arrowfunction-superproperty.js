description('Tests for ES6 arrow function, access to the super property in arrow function');

var expectedValue = 'test-value';

class A {
    getValue () {
        return expectedValue;
    }
};

class B extends A {
    getValueParentFunction() {
        var arrow  = () => super.getValue();
        return arrow();
    }
};

class C extends B {
    constructor(beforeSuper) {
        let _value;
        let arrow = () => super.getValue();
        if (beforeSuper) {
            _value = arrow();
            super();
        } else {
            super();
            _value = arrow();
        }
        this.value = _value;
    }
};

class D {
    constructor() {
        this.value = expectedValue;
    }
    static getStaticValue() {
        return expectedValue;
    }
};

class E extends D {
    static getParentStaticValue() {
        var arrow  = () => super.getStaticValue();
        return arrow();
    }
};

class F extends A {
    constructor() {
        super();
        this.value = expectedValue;
    }
    get prop() {
        var arrow = () => super.getValue()+ '-' + this.value;
        return arrow();
    }
    set prop(value) {
        var arrow = (newVal) => this.value = newVal;
        arrow(value);
    }
    getParentValue() {
        let arrow = () => () => super.getValue();
        return arrow()();
    }
 };

shouldBe('(new B()).getValueParentFunction()', 'expectedValue');

shouldBe('(new C(false)).value', 'expectedValue');

// FIXME: Problem with access to the super before super() in constructor
// https://bugs.webkit.org/show_bug.cgi?id=152108
//shouldThrow('(new C(true))', 'ReferenceError');

shouldBe('E.getParentStaticValue()', 'expectedValue');

var f = new F();

shouldBe('f.prop', 'expectedValue + "-" + expectedValue');

f.prop = 'new-value';
shouldBe('f.prop', 'expectedValue + "-" + "new-value"');

shouldBe('(new F()).getParentValue()', 'expectedValue');

var successfullyParsed = true;
