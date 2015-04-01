
description('Tests for ES6 class syntax expressions');

var constructorCallCount = 0;
const staticMethodValue = [1];
const instanceMethodValue = [2];
const getterValue = [3];
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
shouldThrow("x = class { ( }", "'SyntaxError: Unexpected token \\'(\\'. Expected an identifier.'");
shouldNotThrow("x = class {}");
shouldThrow("x = class { constructor() {} constructor() {} }", "'SyntaxError: Cannot declare multiple constructors in a single class.'");
shouldNotThrow("x = class { constructor() {} static constructor() { return staticMethodValue; } }");
shouldBe("x.constructor()", "staticMethodValue");
shouldThrow("x = class { constructor() {} static prototype() {} }", "'SyntaxError: Cannot declare a static method named \\'prototype\\'.'");
shouldNotThrow("x = class { constructor() {} prototype() { return instanceMethodValue; } }");
shouldBe("(new x).prototype()", "instanceMethodValue");

var successfullyParsed = true;
