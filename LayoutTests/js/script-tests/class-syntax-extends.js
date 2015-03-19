
description('Tests for ES6 class syntax "extends"');

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
shouldBeTrue('(new Derived) instanceof Derived');
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
shouldBe('x.prototype.__proto__', 'Base.prototype');
shouldBe('x = class extends null { constructor() { } }; x.__proto__', 'Function.prototype');
shouldBe('x.__proto__', 'Function.prototype');
shouldThrow('x = class extends 3 { constructor() { } }; x.__proto__', '"TypeError: The superclass is not an object."');
shouldThrow('x = class extends "abc" { constructor() { } }; x.__proto__', '"TypeError: The superclass is not an object."');
shouldNotThrow('baseWithBadPrototype = class { constructor() { } }; baseWithBadPrototype.prototype = 3');
shouldThrow('x = class extends baseWithBadPrototype { constructor() { } }', '"TypeError: The superclass\'s prototype is not an object."');
shouldNotThrow('baseWithBadPrototype.prototype = "abc"');
shouldThrow('x = class extends baseWithBadPrototype { constructor() { } }', '"TypeError: The superclass\'s prototype is not an object."');
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

var successfullyParsed = true;
