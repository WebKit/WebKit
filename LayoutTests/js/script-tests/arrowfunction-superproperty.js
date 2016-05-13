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
shouldBe('(new class extends A { constructor() { ((a = super(), b = super.getValue())=>{ this.id = b; })() } }).id', 'expectedValue');
var expectedNewTarget;
shouldBe('(new class extends A { constructor() { ((a = super(), b = new.target)=>{ this.newTarget = b; })(); expectedNewTarget = new.target;} }).newTarget', 'expectedNewTarget');
shouldThrow('(new class extends A { constructor() { ((a = super.getValue())=>{ this.id = a; })() } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('(new class extends A { constructor() { ((a = super.getValue(), b=super())=>{ this.id = a; })() } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('(new class extends F { constructor() { ((a = super.prop)=>{ return a; })() } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('(new class extends F { constructor() { ((a = super.prop, b=super())=>{ return a; })() } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('(new class extends F { constructor() { ((a = (super.prop = "value"))=>{ this.id = a; })() } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('(new class extends F { constructor() { ((a = (super.prop = "value"), b=super())=>{ this.id = a; })() } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('(new class extends F { constructor() { ((a = super.genGetParentValue().next().value)=>{ this.id = a; })() } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('(new class extends F { constructor() { ((a = super.genGetParentValue().next().value, b=super())=>{ this.id = a; })() } })', '"ReferenceError: Cannot access uninitialized variable."');

var successfullyParsed = true;
