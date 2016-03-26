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
    *genGetParentValue() {
        let arr = () => super.getValue();
        yield arr();
    }
    *genGetParentValueDeepArrow() {
        let arr = () => () => () => super.getValue();
        yield arr()()();
    }
 };

shouldBe('(new B()).getValueParentFunction()', 'expectedValue');

shouldBe('(new C(false)).value', 'expectedValue');

shouldThrow('(new C(true))', '"ReferenceError: Cannot access uninitialized variable."');

shouldBe('E.getParentStaticValue()', 'expectedValue');

var f = new F();

shouldBe('f.prop', 'expectedValue + "-" + expectedValue');

f.prop = 'new-value';
shouldBe('f.prop', 'expectedValue + "-" + "new-value"');

shouldBe('(new F()).getParentValue()', 'expectedValue');

shouldBe('(new F()).genGetParentValue().next().value', 'expectedValue');
shouldBe('(new F()).genGetParentValueDeepArrow().next().value', 'expectedValue');

var successfullyParsed = true;
