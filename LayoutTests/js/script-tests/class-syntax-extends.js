
description('Tests for ES6 class syntax "extends"');

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

function shouldNotThrow(s) {
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

class Base {
    constructor() { }
    baseMethod() { return 'base'; }
    overridenMethod() { return 'base'; }
    static staticBaseMethod() { return 'base'; }
    static staticOverridenMethod() { return 'base'; }
}

class Derived extends Base {
    constructor() { super(); }
    overridenMethod() { return 'derived'; }
    static staticOverridenMethod() { return 'derived'; }
}

shouldBeTrue('(new Base) instanceof Base');
shouldBe('Object.getPrototypeOf(new Base)', 'Base.prototype');
shouldBeTrue('(new Derived) instanceof Derived');
shouldBe('Object.getPrototypeOf(new Derived)', 'Derived.prototype');
shouldBe('Object.getPrototypeOf(Derived.prototype)', 'Base.prototype');
shouldBe('(new Derived).baseMethod()', '"base"');
shouldBe('(new Derived).overridenMethod()', '"derived"');
shouldBe('Derived.staticBaseMethod()', '"base"');
shouldBe('Derived.staticOverridenMethod()', '"derived"');

shouldThrow('x = class extends', '"SyntaxError: Unexpected end of script"');
shouldThrow('x = class extends', '"SyntaxError: Unexpected end of script"');
shouldThrow('x = class extends Base {', '"SyntaxError: Unexpected end of script"');
shouldNotThrow('x = class extends Base { }');
shouldNotThrow('x = class extends Base { constructor() { } }');
shouldBe('x.__proto__', 'Base');
shouldBe('Object.getPrototypeOf(x)', 'Base');
shouldBe('x.prototype.__proto__', 'Base.prototype');
shouldBe('Object.getPrototypeOf(x.prototype)', 'Base.prototype');
shouldBe('x = class extends null { constructor() { } }; x.__proto__', 'Function.prototype');
shouldBe('x.__proto__', 'Function.prototype');
shouldThrow('x = class extends 3 { constructor() { } }; x.__proto__', '"TypeError: The superclass is not a constructor."');
shouldThrow('x = class extends "abc" { constructor() { } }; x.__proto__', '"TypeError: The superclass is not a constructor."');
shouldNotThrow('baseWithBadPrototype = function () {}; baseWithBadPrototype.prototype = 3; new baseWithBadPrototype');
shouldThrow('x = class extends baseWithBadPrototype { constructor() { } }', '"TypeError: The value of the superclass\'s prototype property is not an object or null."');
shouldNotThrow('baseWithBadPrototype.prototype = "abc"');
shouldThrow('x = class extends baseWithBadPrototype { constructor() { } }', '"TypeError: The value of the superclass\'s prototype property is not an object or null."');
shouldNotThrow('baseWithBadPrototype.prototype = null; x = class extends baseWithBadPrototype { constructor() { } }');

shouldThrow('x = 1; c = class extends ++x { constructor() { } };');
shouldThrow('x = 1; c = class extends x++ { constructor() { } };');
shouldThrow('x = 1; c = class extends (++x) { constructor() { } };');
shouldThrow('x = 1; c = class extends (x++) { constructor() { } };');
shouldBe('x = 1; try { c = class extends (++x) { constructor() { } } } catch (e) { }; x', '2');
shouldBe('x = 1; try { c = class extends (x++) { constructor() { } } } catch (e) { }; x', '2');

shouldNotThrow('namespace = {}; namespace.A = class { }; namespace.B = class extends namespace.A { }');
shouldNotThrow('namespace = {}; namespace.A = class A { }; namespace.B = class B extends namespace.A { }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends namespace.A { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class A { constructor() { } }; namespace.B = class B extends namespace.A { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends (namespace.A) { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends namespace["A"] { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; function getClassA() { return namespace.A }; namespace.B = class extends getClassA() { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; function getClass(prop) { return namespace[prop] }; namespace.B = class extends getClass("A") { constructor() { } }');
shouldNotThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends (false||null||namespace.A) { constructor() { } }');
shouldThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends false||null||namespace.A { constructor() { } }');
shouldNotThrow('x = 1; namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends (x++, namespace.A) { constructor() { } };');
shouldThrow('x = 1; namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends (namespace.A, x++) { constructor() { } };');
shouldThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends new namespace.A { constructor() { } }');
shouldThrow('namespace = {}; namespace.A = class { constructor() { } }; namespace.B = class extends new namespace.A() { constructor() { } }');
shouldBe('x = 1; namespace = {}; namespace.A = class { constructor() { } }; try { namespace.B = class extends (x++, namespace.A) { constructor() { } } } catch (e) { } x', '2');
shouldBe('x = 1; namespace = {}; namespace.A = class { constructor() { } }; try { namespace.B = class extends (namespace.A, x++) { constructor() { } } } catch (e) { } x', '2');

shouldBe('Object.getPrototypeOf((class { constructor () { } }).prototype)', 'Object.prototype');
shouldBe('Object.getPrototypeOf((class extends null { constructor () { super(); } }).prototype)', 'null');
shouldThrow('new (class extends undefined { constructor () { this } })', '"TypeError: The superclass is not a constructor."');
shouldThrow('x = undefined; new (class extends x { constructor () { super(); } })', '"TypeError: The superclass is not a constructor."');
shouldBeTrue ('class x {}; new (class extends null { constructor () { return new x; } }) instanceof x');
shouldThrow('new (class extends null { constructor () { this; } })');
shouldThrow('new (class extends null { constructor () { super(); } })', '"TypeError: function is not a constructor (evaluating \'super()\')"');
shouldBe('x = {}; new (class extends null { constructor () { return x } })', 'x');
shouldThrow('y = 12; class C extends null { constructor () { return y; } }; new C;', '"TypeError: Cannot return a non-object type in the constructor of a derived class."');
shouldBeTrue ('class x {}; new (class extends null { constructor () { return new x; } }) instanceof x');
shouldBe('x = null; Object.getPrototypeOf((class extends x { }).prototype)', 'null');
shouldBeTrue('Object.prototype.isPrototypeOf(class { })');
shouldBeTrue('Function.prototype.isPrototypeOf(class { })');

var successfullyParsed = true;
