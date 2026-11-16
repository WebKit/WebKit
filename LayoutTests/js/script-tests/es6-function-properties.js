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

 var typeErrorText = '"TypeError: \'arguments\', \'callee\', and \'caller\' cannot be accessed in this context."';

class A { };
class B extends A { };
class C extends A { constructor () {super();}}
class D { constructor () {}}
class E { getItem() { return 'text';} }
class F extends E { getElement() { return 'text'}}
class G extends E { get item() { return 'text'} }

var wrongPropertyInAList = '';

var check = function (text) {
    shouldThrow(text + '.caller', typeErrorText);
    shouldThrow(text + '.arguments', typeErrorText);
    shouldThrow(text + '["caller"]', typeErrorText);
    shouldThrow(text + '["arguments"]', typeErrorText);
    shouldThrow(text + '.caller = function () {}', typeErrorText);
    shouldThrow(text + '.arguments = function () {}', typeErrorText);
    shouldThrow(text + '["caller"] = function () {}', typeErrorText);
    shouldThrow(text + '["arguments"] = function () {}', typeErrorText);

    shouldBe(text + '.hasOwnProperty("caller")', 'false');
    shouldBe(text + '.hasOwnProperty("arguments")', 'false');
    var object = eval(text);

    for (var propertyName in object) {
        wrongPropertyInAList = propertyName === 'caller' || propertyName === 'arguments';
        if (wrongPropertyInAList)
           shouldBe('wrongPropertyInAList', 'false');
    }

    shouldBe('Object.getOwnPropertyDescriptor(' + text + ', "caller")', 'undefined');
    shouldBe('Object.getOwnPropertyDescriptor(' + text + ', "arguments")', 'undefined');
    shouldBe('delete ' + text +'.caller', true);
    shouldBe('delete ' + text +'["caller"]', true);
    shouldBe('delete ' + text +'.arguments', true);
    shouldBe('delete ' + text +'["arguments"]', true);
}

var checkProperties = function (value, expected, text) { 
    debug(text);
    var properties = Object.getOwnPropertyNames(value).toString();
    shouldBe("'" + properties + "'", "'" + expected + "'");
};

var a = new A;

checkProperties(a.constructor, "length,name,prototype", "a.constructor");
checkProperties((new A()).constructor, "length,name,prototype", "(new A()).constructor");

check('a.constructor');
check('a.constructor');
check('(new A()).constructor');

var b = new B;

checkProperties(b.constructor, "length,name,prototype", "b.constructor");
checkProperties((new B()).constructor, "length,name,prototype", "(new B()).constructor");

check('b.constructor');
check('(new B()).constructor');

var c = new C;

checkProperties(c.constructor, "length,name,prototype", "c.constructor");
checkProperties((new C()).constructor, "length,name,prototype", "(new C()).constructor");

check('c.constructor');
check('(new C()).constructor');

var d = new D;

checkProperties(d.constructor, "length,name,prototype", "c.constructor");
checkProperties((new D()).constructor, "length,name,prototype", "(new D()).constructor");

check('d.constructor');
check('(new D()).constructor');

var e = new E;

checkProperties(e.constructor, "length,name,prototype", "e.constructor");
checkProperties((new E()).getItem, "length,name", "(new E()).getItem");

check('e.constructor');
check('(new E()).getItem');

var f = new F;

checkProperties(f.getItem, "length,name", "f.getItem");
checkProperties(f.getElement, "length,name", "f.getElement");

check('f.getItem');
check('f.getElement');

checkProperties((new F()).getItem, "length,name", "(new F()).getItem");
checkProperties((new F()).getElement, "length,name", "(new F()).getElement");

check('(new F()).getItem');
check('(new F()).getElement');

var arr = ()=>{};

checkProperties(arr, "length,name", "arr");
checkProperties((()=>{}), "length,name", "()=>{}");

check('arr');
check('(()=>{})');

var g = new G;
shouldBe('g.item.caller', 'undefined');
shouldBe('g.item.arguments', 'undefined');
shouldBe('(new G()).item.caller', 'undefined');
shouldBe('(new G()).item.arguments', 'undefined');

class H {
    caller() { return 'value'; }
    arguments() { return 'value'; }
}

check('H');

var h = new H;

shouldBe('h.caller()', '"value"');
shouldBe('h.arguments()', '"value"');

checkProperties(h.caller, "length,name", "h.caller");
checkProperties(h.arguments, "length,name", "h.arguments");

check('h.caller');

shouldBe('(new H()).caller()', '"value"');
shouldBe('(new H()).arguments()', '"value"');

check('(new H()).caller', "length,name,prototype");

class J {
    *gen() {yield;}
    static *gen() {yield;}
    *get() {}
    static *get() {}
}

checkProperties(J.gen, "length,name,prototype", "J.gen");
checkProperties(J.get, "length,name,prototype", "J.get");

check('J.gen');
check('J.get');

var j = new J;

checkProperties(j.gen, "length,name,prototype", "j.gen");
checkProperties(j.get, "length,name,prototype", "j.get");

check('j.gen');
check('j.get');

checkProperties((new J).gen, "length,name,prototype", "(new J).gen");
checkProperties((new J).get, "length,name,prototype", "(new J).get");

check('(new J).gen');
check('(new J).get');

var k = {
    method() {},
    *gen() {},
    get getter() { },
    set setter(v) { }
};

checkProperties(k.method, "length,name", "k.method");
check("k.method");

checkProperties(k.gen, "length,name,prototype", "k.gen");
check("k.gen");

checkProperties(Object.getOwnPropertyDescriptor(k, "getter").get, "length,name", "k.getter");
check("Object.getOwnPropertyDescriptor(k, 'getter').get");

checkProperties(Object.getOwnPropertyDescriptor(k, "setter").set, "length,name", "k.setter");
check("Object.getOwnPropertyDescriptor(k, 'setter').set");

var successfullyParsed = true;
