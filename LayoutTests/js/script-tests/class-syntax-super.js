
description('Tests for ES6 class syntax "super"');

function shouldThrow(s, message) {
    var threw = false;
    try {
        eval(s);
    } catch(e) {
        threw = true;
        if (!message || e.toString() === eval(message))
            testPassed(s + ":::" + e.toString());
        else
            testFailed(s);
    }
    if (!threw)
        testFailed(s);
}

function shouldNotThrow(s, message) {
    var threw = false;
    try {
        eval(s);
    } catch(e) {
        threw = true;
    }
    if (threw)
        testFailed(s);
    else
        testPassed(s);
}

function shouldBe(a, b) {
    if (eval(a) === eval(b))
        testPassed(a + ":::" + b);
    else
        testFailed(a + ":::" + b);
}

function shouldBeTrue(s) {
    if (eval(s) === true)
        testPassed(s);
    else
        testFailed(s);
}

function shouldBeFalse(s) {
    if (eval(s) === false)
        testPassed(s);
    else
        testFailed(s);
}

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
    set callBaseMethodInSetter(x) { valueInSetter = super.baseMethod(); }
    get baseMethodInGetterSetter() { return super.baseMethod; }
    set baseMethodInGetterSetter(x) { valueInSetter = super['baseMethod']; }
    static staticMethod() { return super.staticMethod(); }
}

class SecondDerived extends Derived {
    constructor() { super(); }
    chainMethod() { return super.chainMethod().concat(['secondDerived']); }
}

class DerivedWithEval extends Base {
    constructor(throwTDZ) {
      if (throwTDZ)
        this.id = '';

      eval("super()");
    }
}

shouldBeTrue('(new Base) instanceof Base');
shouldBeTrue('(new Derived) instanceof Derived');
shouldBeTrue('(new DerivedWithEval) instanceof DerivedWithEval');
shouldThrow('(new DerivedWithEval(true))', '"ReferenceError: Cannot access uninitialized variable."');
shouldBe('(new Derived).callBaseMethod()', 'baseMethodValue');
shouldBe('x = (new Derived).callBaseMethod; x()', 'baseMethodValue');
shouldBe('(new Derived).callBaseMethodInGetter', 'baseMethodValue');
shouldBe('(new Derived).callBaseMethodInSetter = 1; valueInSetter', 'baseMethodValue');
shouldBe('(new Derived).baseMethodInGetterSetter', '(new Base).baseMethod');
shouldBe('(new Derived).baseMethodInGetterSetter = 1; valueInSetter', '(new Base).baseMethod');
shouldBe('Derived.staticMethod()', '"base3"');
shouldBe('(new SecondDerived).chainMethod().toString()', '["base", "derived", "secondDerived"].toString()');
shouldNotThrow('x = class extends Base { constructor() { super(); } super() {} }');
shouldThrow('x = class extends Base { constructor() { super(); } method() { super() } }',
    '"SyntaxError: super is not valid in this context."');
shouldThrow('x = class extends Base { constructor() { super(); } method() { super } }', '"SyntaxError: super is not valid in this context."');
shouldThrow('x = class extends Base { constructor() { super(); } method() { return new super } }', '"SyntaxError: Cannot use new with super call."');
shouldNotThrow('x = class extends Base { constructor() { super(); } method1() { delete (super.foo) } method2() { delete super["foo"] } }');
shouldThrow('(new x).method1()', '"ReferenceError: Cannot delete a super property"');
shouldThrow('(new x).method2()', '"ReferenceError: Cannot delete a super property"');
shouldBeTrue('(new (class { constructor() { super.property = "ABC"; } })).property === "ABC"');
shouldBeTrue('(new (class extends Base { constructor() { super(); super.property = "ABC"; } })).property === "ABC"');
shouldBeTrue('(new (class { constructor() { var arr = () => super.property = "ABC"; arr(); } })).property === "ABC"');
shouldBeTrue('(new (class { constructor() { var async_arr = async () => super.property = "ABC"; async_arr(); } })).property === "ABC"');
shouldBeTrue('(new (class { constructor() { eval(\'super.property = "ABC"\'); } })).property === "ABC"');
shouldBeTrue('(new (class { constructor() { var arr = () => eval(\'super.property = "ABC"\'); arr(); } })).property === "ABC"');
shouldBeTrue('new (class { constructor() { return undefined; } }) instanceof Object');
shouldBeTrue('new (class { constructor() { return 1; } }) instanceof Object');
shouldThrow('new (class extends Base { constructor() { return undefined } })');
shouldBeTrue('new (class extends Base { constructor() { super(); return undefined } }) instanceof Object');
shouldBe('x = { }; new (class extends Base { constructor() { return x } });', 'x');
shouldBeFalse('x instanceof Base');
shouldThrow('new (class extends Base { constructor() { } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('new (class extends Base { constructor() { return 1; } })', '"TypeError: Cannot return a non-object type in the constructor of a derived class."');
shouldThrow('new (class extends null { constructor() { return undefined } })');
shouldThrow('new (class extends null { constructor() { super(); return undefined } })', '"TypeError: function is not a constructor (evaluating \'super()\')"');
shouldBe('x = { }; new (class extends null { constructor() { return x } });', 'x');
shouldBeTrue('x instanceof Object');
shouldThrow('new (class extends null { constructor() { } })');
shouldThrow('new (class extends null { constructor() { return 1; } })');
shouldThrow('new (class extends null { constructor() { super() } })', '"TypeError: function is not a constructor (evaluating \'super()\')"');
shouldThrow('new (class { constructor() { super() } })', '"SyntaxError: super is not valid in this context."');
shouldThrow('function x() { super(); }', '"SyntaxError: super is not valid in this context."');
shouldThrow('new (class extends Object { constructor() { function x() { super() } } })', '"SyntaxError: super is not valid in this context."');
shouldThrow('new (class extends Object { constructor() { function x() { super.method } } })', '"SyntaxError: super is not valid in this context."');
shouldThrow('function x() { super.method(); }', '"SyntaxError: super is not valid in this context."');
shouldThrow('function x() { super(); }', '"SyntaxError: super is not valid in this context."');
shouldThrow('eval("super.method()")', '"SyntaxError: super is not valid in this context."');
shouldThrow('eval("super()")', '"SyntaxError: super is not valid in this context."');

shouldThrow('(function () { eval("super.method()");})()', '"SyntaxError: super is not valid in this context."');
shouldThrow('(function () { eval("super()");})()', '"SyntaxError: super is not valid in this context."');

shouldThrow('new (class { constructor() { (function () { eval("super()");})(); } })', '"SyntaxError: super is not valid in this context."');
shouldThrow('(new (class { method() { (function () { eval("super.method()");})(); }})).method()', '"SyntaxError: super is not valid in this context."');

shouldThrow('new (class extends Base { constructor() { super(); super();}})', '"ReferenceError: \'super()\' can\'t be called more than once in a constructor."');
shouldThrow('(new class D extends class { m() {}} { constructor() { eval(\'super["m"]()\') } })', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('new class extends class { m() {}} { constructor() { super["m"](super()) } }', '"ReferenceError: Cannot access uninitialized variable."');
shouldThrow('(new class D extends class { m() {}} { constructor(f) { super[f()]() } }(()=>"m"))', '"ReferenceError: Cannot access uninitialized variable."');

shouldNotThrow('(new class D extends class { m() {}} { constructor() { super(); eval(\'super["m"]()\') } })');
shouldThrow('new class extends class { m() {}} { constructor() { super(); super["m"](super()) } }', '"ReferenceError: \'super()\' can\'t be called more than once in a constructor."');
shouldNotThrow('(new class D extends class { m() {}} { constructor(f) { super(); super[f()]() } }(()=>"m"))');

var successfullyParsed = true;
