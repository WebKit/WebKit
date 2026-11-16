
description('Tests for ES6 class syntax declarations');

function shouldThrow(s, message) {
    var threw = false;
    try {
        eval(s);
    } catch(e) {
        threw = true;
        if (!message || e.toString() === eval(message))
            testPassed(s + ":::" + e.toString());
        else
            testFailed(s + ":::" + e.toString() + ":::" + message);
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

var constructorCallCount = 0;
var staticMethodValue = [1];
var instanceMethodValue = [2];
var getterValue = [3];
var setterValue = undefined;
class A {
    constructor() { constructorCallCount++; }
    static someStaticMethod() { return staticMethodValue; }
    static get someStaticGetter() { return getterValue; }
    static set someStaticSetter(value) { setterValue = value; }
    someInstanceMethod() { return instanceMethodValue; }
    get someGetter() { return getterValue; }
    set someSetter(value) { setterValue = value; }
}

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

shouldThrow("class", "'SyntaxError: Unexpected end of script'");
shouldThrow("class [", "'SyntaxError: Unexpected token \\'[\\''");
shouldThrow("class {", "'SyntaxError: Class statements must have a name.'");
shouldThrow("class X {", "'SyntaxError: Unexpected end of script'");
shouldThrow("class X { ( }", "'SyntaxError: Unexpected token \\'(\\''");
shouldNotThrow("class X {}");

shouldThrow("class X { constructor() {} constructor() {} }", "'SyntaxError: Cannot declare multiple constructors in a single class.'");
shouldThrow("class X { get constructor() {} }", "'SyntaxError: Cannot declare a getter or setter named \\'constructor\\'.'");
shouldThrow("class X { set constructor() {} }", "'SyntaxError: Cannot declare a getter or setter named \\'constructor\\'.'");
shouldNotThrow("class X { ['constructor']() {} }");
shouldNotThrow("class X { ['constructor']() { throw 'unreached' } }; new X");
shouldNotThrow("class X { constructor() {} static constructor() { return staticMethodValue; } }");
shouldBe("class X { constructor() {} static constructor() { return staticMethodValue; } }; X.constructor()", "staticMethodValue");
shouldNotThrow("class X { constructor() {} static get constructor() { return staticMethodValue; } }");
shouldBe("class X { constructor() {} static get constructor() { return staticMethodValue; } }; X.constructor", "staticMethodValue");

shouldThrow("class X { constructor() {} static prototype() {} }", "'SyntaxError: Cannot declare a static method named \\'prototype\\'.'");
shouldThrow("class X { constructor() {} static get prototype() {} }", "'SyntaxError: Cannot declare a static method named \\'prototype\\'.'");
shouldThrow("class X { constructor() {} static set prototype() {} }", "'SyntaxError: Cannot declare a static method named \\'prototype\\'.'");
shouldThrow("class X { constructor() {} static get ['prototype']() {} }", "'TypeError: Attempting to change configurable attribute of unconfigurable property.'");
shouldThrow("class X { constructor() {} static set ['prototype'](x) {} }", "'TypeError: Attempting to change configurable attribute of unconfigurable property.'");
shouldNotThrow("class X { constructor() {} prototype() { return instanceMethodValue; } }");
shouldNotThrow("class X { constructor() {} get prototype() { return instanceMethodValue; } }");
shouldNotThrow("class X { constructor() {} set prototype(x) { } }");
shouldBe("class X { constructor() {} prototype() { return instanceMethodValue; } }; (new X).prototype()", "instanceMethodValue");

shouldNotThrow("class X { constructor() {} set foo(a) {} }");
shouldNotThrow("class X { constructor() {} set foo({x, y}) {} }");
shouldThrow("class X { constructor() {} set foo() {} }");
shouldThrow("class X { constructor() {} set foo(a, b) {} }");
shouldNotThrow("class X { constructor() {} get foo() {} }");
shouldThrow("class X { constructor() {} get foo(x) {} }");
shouldThrow("class X { constructor() {} get foo({x, y}) {} }");

var successfullyParsed = true;
