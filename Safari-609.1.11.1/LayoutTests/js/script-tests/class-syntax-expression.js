
description('Tests for ES6 class syntax expressions');

var constructorCallCount = 0;
var staticMethodValue = [1];
var instanceMethodValue = [2];
var getterValue = [3];
var setterValue = undefined;
var A = class {
    constructor() { constructorCallCount++; }
    static someStaticMethod() { return staticMethodValue; }
    static get someStaticGetter() { return getterValue; }
    static set someStaticSetter(value) { setterValue = value; }
    someInstanceMethod() { return instanceMethodValue; }
    get someGetter() { return getterValue; }
    set someSetter(value) { setterValue = value; }
};

shouldBe("constructorCallCount", "0");
shouldBe("A.someStaticMethod()", "staticMethodValue");
shouldBe("A.someStaticGetter", "getterValue");
shouldBe("setterValue = undefined; A.someStaticSetter = 123; setterValue", "123");
shouldBe("(new A).someInstanceMethod()", "instanceMethodValue");
shouldBe("constructorCallCount", "1");
shouldBe("(new A).someGetter", "getterValue");
shouldBe("constructorCallCount", "2");
shouldBe("(new A).someGetter", "getterValue");
shouldBe("setterValue = undefined; (new A).someSetter = 789; setterValue", "789");
shouldBe("(new A).__proto__", "A.prototype");
shouldBe("A.prototype.constructor", "A");

shouldThrow("x = class", "'SyntaxError: Unexpected end of script'");
shouldThrow("x = class {", "'SyntaxError: Unexpected end of script'");
shouldThrow("x = class { ( }", "'SyntaxError: Unexpected token \\'(\\''");
shouldNotThrow("x = class {}");

shouldThrow("x = class { constructor() {} constructor() {} }", "'SyntaxError: Cannot declare multiple constructors in a single class.'");
shouldThrow("x = class { get constructor() {} }", "'SyntaxError: Cannot declare a getter or setter named \\'constructor\\'.'");
shouldThrow("x = class { set constructor() {} }", "'SyntaxError: Cannot declare a getter or setter named \\'constructor\\'.'");
shouldNotThrow("x = class { constructor() {} static constructor() { return staticMethodValue; } }");
shouldBe("x = class { constructor() {} static constructor() { return staticMethodValue; } }; x.constructor()", "staticMethodValue");

shouldThrow("x = class { constructor() {} static prototype() {} }", "'SyntaxError: Cannot declare a static method named \\'prototype\\'.'");
shouldThrow("x = class { constructor() {} static get prototype() {} }", "'SyntaxError: Cannot declare a static method named \\'prototype\\'.'");
shouldThrow("x = class { constructor() {} static set prototype() {} }", "'SyntaxError: Cannot declare a static method named \\'prototype\\'.'");
shouldNotThrow("x = class  { constructor() {} prototype() { return instanceMethodValue; } }");
shouldBe("x = class { constructor() {} prototype() { return instanceMethodValue; } }; (new x).prototype()", "instanceMethodValue");

shouldNotThrow("x = class { constructor() {} set foo(a) {} }");
shouldNotThrow("x = class { constructor() {} set foo({x, y}) {} }");
shouldThrow("x = class { constructor() {} set foo() {} }");
shouldThrow("x = class { constructor() {} set foo(a, b) {} }");
shouldNotThrow("x = class { constructor() {} get foo() {} }");
shouldThrow("x = class { constructor() {} get foo(x) {} }");
shouldThrow("x = class { constructor() {} get foo({x, y}) {} }");

var successfullyParsed = true;
