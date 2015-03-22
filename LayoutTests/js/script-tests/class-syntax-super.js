
description('Tests for ES6 class syntax "super"');

var baseMethodValue = {};
var valueInSetter = null;

class Base {
    constructor() { }
    baseMethod() { return baseMethodValue; }
    chainMethod() { return 'base'; }
    static staticMethod() { return 'base3'; }
}

class Derived extends Base {
    constructor() { super(); }
    chainMethod() { return [super.chainMethod(), 'derived']; }
    callBaseMethod() { return super.baseMethod(); }
    get callBaseMethodInGetter() { return super['baseMethod'](); }
    set callBaseMethodInSetter() { valueInSetter = super.baseMethod(); }
    get baseMethodInGetterSetter() { return super.baseMethod; }
    set baseMethodInGetterSetter() { valueInSetter = super['baseMethod']; }
    static staticMethod() { return super.staticMethod(); }
}

class SecondDerived extends Derived {
    constructor() { super(); }
    chainMethod() { return super.chainMethod().concat(['secondDerived']); }
}

shouldBeTrue('(new Base) instanceof Base');
shouldBeTrue('(new Derived) instanceof Derived');
shouldBe('(new Derived).callBaseMethod()', 'baseMethodValue');
shouldBe('x = (new Derived).callBaseMethod; x()', 'baseMethodValue');
shouldBe('(new Derived).callBaseMethodInGetter', 'baseMethodValue');
shouldBe('(new Derived).callBaseMethodInSetter = 1; valueInSetter', 'baseMethodValue');
shouldBe('(new Derived).baseMethodInGetterSetter', '(new Base).baseMethod');
shouldBe('(new Derived).baseMethodInGetterSetter = 1; valueInSetter', '(new Base).baseMethod');
shouldBe('Derived.staticMethod()', '"base3"');
shouldBe('(new SecondDerived).chainMethod()', '["base", "derived", "secondDerived"]');
shouldThrow('x = class extends Base { constructor() { super(); } super() {} }', '"SyntaxError: Unexpected keyword \'super\'. Expected an identifier."');
shouldThrow('x = class extends Base { constructor() { super(); } method() { super() } }',
    '"SyntaxError: Cannot call super() outside of a class constructor."');
shouldThrow('x = class extends Base { constructor() { super(); } method() { super } }', '"SyntaxError: Cannot reference super."');
shouldNotThrow('x = class extends Base { constructor() { super(); } method() { return new super } }');
shouldBeTrue('(new x).method() instanceof Base');
shouldBeFalse('(new x).method() instanceof x');
shouldNotThrow('x = class extends Base { constructor() { super(); } method1() { delete (super.foo) } method2() { delete super["foo"] } }');
shouldThrow('(new x).method1()', '"ReferenceError: Cannot delete a super property"');
shouldThrow('(new x).method2()', '"ReferenceError: Cannot delete a super property"');
shouldBeTrue('new (class { constructor() { return undefined; } }) instanceof Object');
shouldBeTrue('new (class { constructor() { return 1; } }) instanceof Object');
shouldBe('new (class extends Base { constructor() { return undefined } })', 'undefined');
shouldBe('x = { }; new (class extends Base { constructor() { return x } });', 'x');
shouldBeFalse('x instanceof Base');
shouldThrow('new (class extends Base { constructor() { } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('new (class extends Base { constructor() { return 1; } })', '"TypeError: Cannot return a non-object type in the constructor of a derived class."');
shouldBe('new (class extends null { constructor() { return undefined } })', 'undefined');
shouldBe('x = { }; new (class extends null { constructor() { return x } });', 'x');
shouldBeTrue('x instanceof Object');
shouldThrow('new (class extends null { constructor() { } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('new (class extends null { constructor() { return 1; } })', '"TypeError: Cannot return a non-object type in the constructor of a derived class."');
shouldNotThrow('new (class extends null { constructor() { super() } })');
shouldThrow('new (class { constructor() { super() } })', '"SyntaxError: Cannot call super() in a base class constructor."');
shouldThrow('function x() { super(); }', '"SyntaxError: Cannot call super() outside of a class constructor."');
shouldThrow('new (class extends Object { constructor() { function x() { super() } } })', '"SyntaxError: Cannot call super() outside of a class constructor."');
shouldThrow('new (class extends Object { constructor() { function x() { super.method } } })', '"SyntaxError: super can only be used in a method of a derived class."');
shouldThrow('function x() { super.method(); }', '"SyntaxError: super can only be used in a method of a derived class."');
shouldThrow('function x() { super(); }', '"SyntaxError: Cannot call super() outside of a class constructor."');

var successfullyParsed = true;
