
description('Tests for ES6 class method and constructor properties');

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

function shouldBe(a, b) {
    var r1 = eval(a);
    var r2 = eval(b)
    if (r1 === r2)
        testPassed(a + ":::" + b);
    else
        testFailed(r1 + ":::" + r2);
}

class A { };
class B extends A { };
class C extends A { constructor () {super();}}
class D { constructor () {}}
class E { getItem() { return 'text';} }
class F extends E { getElement() { return 'text'}}
class G extends E { get item() { return 'text'} }

shouldThrow('(new A()).constructor.caller', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new A()).constructor.arguments', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new B()).constructor.caller', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new B()).constructor.arguments', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new C()).constructor.caller', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new C()).constructor.arguments', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new D()).constructor.caller', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new D()).constructor.arguments', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new E()).getItem.caller', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new E()).getItem.arguments', '');
shouldThrow('(new F()).getItem.caller', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new F()).getItem.arguments', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new F()).getElement.caller', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new F()).getElement.arguments', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');

shouldBe('(new G()).item.caller', 'undefined');
shouldBe('(new G()).item.arguments', 'undefined');

class H {
  caller() { return 'value'; }
  arguments() { return 'value'; }
}

shouldThrow('H.caller', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('H.arguments', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');

shouldBe('(new H()).caller()', '"value"');
shouldBe('(new H()).arguments()', '"value"');

shouldThrow('(new H()).caller.caller', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');
shouldThrow('(new H()).caller.arguments', '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in strict mode."');


var successfullyParsed = true;
