description("basic tests for object literal duplicate properties");

// Ensure basic properties get eliminated by getters.
x = 0;
o = {
    foo: x++,
    get foo() {return "getter"},
};
shouldBe("x", "1");
shouldBe("o.foo", "'getter'");

// Ensure getters/setters get eliminated by basic properties.
x = 0;
o = {
    get foo() {return "getter"},
    foo: x++,
};
shouldBe("x", "1");
shouldBe("o.foo", "0");

// Ensure computed properties are eliminated by getters/setters.
x = 0;
o = {
    ['foo']: x++,
    get foo() {return "getter"},
};
shouldBe("x", "1");
shouldBe("o.foo", "'getter'");

// Ensure getters/setters properties are eliminated by computed properties.
x = 0;
o = {
    get foo() {return "getter"},
    set foo(x) {},
    ['foo']: x++,
};
shouldBe("x", "1");
shouldBe("o.foo", "0");

// Multiple types and multiple properties.
x = 0;
o = {
    get foo() { return "NO"; },
    foo: x++,
    set test1(x) {},
    bar: x++,
    get test2() {},
    ['test3']: x++,
    get foo() {return "getter"},
    ['foo']: x++,
    set bar(x) {},
    nest: {foo:1, get foo(){}, bar:1, set foo(x){}},
};
shouldBe("x", "4");
shouldBe("o.foo", "3");
shouldBe("o.bar", "undefined");
shouldBe("Object.keys(o).join()", "'foo,test1,bar,test2,test3,nest'");



function descriptionString(o, property) {
    var descriptor = Object.getOwnPropertyDescriptor(o, property);
    if (!descriptor)
        return "PROPERTY NOT FOUND";

    var string = "";
    if (descriptor.value)
        string += "value:" + String(descriptor.value) + " ";
    if (descriptor.get)
        string += "getter:" + typeof descriptor.get + " value:(" + o[property] + ") ";
    if (descriptor.set)
        string += "setter:" + typeof descriptor.set + " ";

    string += "keys:" + Object.keys(o).length + " ";

    string += "[";
    if (descriptor.enumerable)
        string += "E";
    if (descriptor.configurable)
        string += "C";
    if (descriptor.writable)
        string += "W";
    string += "]";

    if (Object.isSealed(o))
        string += "[Sealed]";
    if (Object.isFrozen(o))
        string += "[Frozen]";
    if (Object.isExtensible(o))
        string += "[Extensible]";

    return string;
}

function runTest(test, expected) {
    test = test + "; descriptionString(o, 'foo');";
    if (expected) {
        shouldBe(test, expected);
        shouldBe("'use strict';" + test, expected);
        shouldNotThrow("(function(){" + test + "})()");
    } else {
        shouldThrow(test);
        shouldThrow("'use strict';" + test);
        shouldThrow("(function(){" + test + "})()");
    }
}

// Basic properties.
debug(""); debug("Basic");
runTest("o = {foo:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1}; o.foo = 2", "'value:2 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1, foo:3}", "'value:3 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1, fooo:2, foo:3}", "'value:3 keys:2 [ECW][Extensible]'");
runTest("o = {foo:1, fooo:2, foo:3}; o.foo = 4", "'value:4 keys:2 [ECW][Extensible]'");
runTest("o = {foo:1, fooo:2, foo:3, bar: 9}", "'value:3 keys:3 [ECW][Extensible]'");
runTest("o = {foo:1, foo:3}; Object.defineProperty(o, 'foo', {value:5})", "'value:5 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1, foo:3}; Object.defineProperty(o, 'foo', {get(){return 5}})", "'getter:function value:(5) keys:1 [EC][Extensible]'");
runTest("o = {foo:1, foo:3}; o.__defineGetter__('foo', function(){return 5})", "'getter:function value:(5) keys:1 [EC][Extensible]'");
runTest("o = {foo:1, foo:3}; Object.seal(o);", "'value:3 keys:1 [EW][Sealed]'");
runTest("o = {foo:1, foo:3}; Object.seal(o); o.foo = 5", "'value:5 keys:1 [EW][Sealed]'");
runTest("o = {foo:1, foo:3}; Object.seal(o); Object.defineProperty(o, 'foo', {value:5})", "'value:5 keys:1 [EW][Sealed]'");
runTest("o = {foo:1, foo:3}; Object.seal(o); Object.defineProperty(o, 'foo', {get(){return 5}})", null);
runTest("o = {foo:1, foo:3}; Object.seal(o); o.__defineGetter__('foo', function(){return 5})");

// Basic properties with Computed properties.
debug(""); debug("Basic + Computed");
runTest("o = {['foo']:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1, ['foo']:2}", "'value:2 keys:1 [ECW][Extensible]'");
runTest("o = {['foo']:1, foo:2}", "'value:2 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1, ['foo']:2, foo:3}", "'value:3 keys:1 [ECW][Extensible]'");
runTest("o = {['foo']:1, foo:2, ['foo']:3}", "'value:3 keys:1 [ECW][Extensible]'");
runTest("o = {['foo']:1, ['foo']:2}", "'value:2 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1, ['foo']:2}; o.foo = 3", "'value:3 keys:1 [ECW][Extensible]'");
runTest("o = {['foo']:1, ['bar']:2}", "'value:1 keys:2 [ECW][Extensible]'");
runTest("o = {['foo']:1, ['bar']:2, foo:3}", "'value:3 keys:2 [ECW][Extensible]'");

// Basic properties with Accessor properties.
debug(""); debug("Basic + Accessor");
runTest("o = {get foo(){return 2}}", "'getter:function value:(2) keys:1 [EC][Extensible]'");
runTest("o = {set foo(x){}}", "'setter:function keys:1 [EC][Extensible]'");
runTest("o = {get foo(){return 1}, get foo(){return 2}}", "'getter:function value:(2) keys:1 [EC][Extensible]'");
runTest("o = {get foo(){return 2}, set foo(x){}}", "'getter:function value:(2) setter:function keys:1 [EC][Extensible]'");
runTest("o = {get foo(){return 2}, set foo(x){}, get foo(){return 3}}", "'getter:function value:(3) setter:function keys:1 [EC][Extensible]'");
runTest("o = {bar:1, get foo(){return 2}, set foo(x){}, baz:1}", "'getter:function value:(2) setter:function keys:3 [EC][Extensible]'");
runTest("o = {foo:1, get foo(){return 2}}", "'getter:function value:(2) keys:1 [EC][Extensible]'");
runTest("o = {foo:1, get foo(){return 2}, set foo(x){}}", "'getter:function value:(2) setter:function keys:1 [EC][Extensible]'");
runTest("o = {get foo(){return 2}, foo:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {get foo(){return 2}, set foo(x){}, foo:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1, get foo(){return -1}, foo:2}", "'value:2 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1, get foo(){return 2}, bar:3}", "'getter:function value:(2) keys:2 [EC][Extensible]'");
runTest("o = {foo:1, get foo(){return -1}, foo:-1, get foo() {return 2}}", "'getter:function value:(2) keys:1 [EC][Extensible]'");
runTest("o = {foo:1, get foo(){return 3}}; Object.defineProperty(o, 'foo', {value:5})", "'value:5 keys:1 [EC][Extensible]'");
runTest("o = {foo:1, get foo(){return 3}}; Object.defineProperty(o, 'foo', {get(){return 5}})", "'getter:function value:(5) keys:1 [EC][Extensible]'");
runTest("o = {foo:1, get foo(){return 3}}; o.__defineGetter__('foo', function(){return 5})", "'getter:function value:(5) keys:1 [EC][Extensible]'");
runTest("o = {foo:1, get foo(){return 3}, set foo(x){}}; Object.seal(o); o.foo = 5", "'getter:function value:(3) setter:function keys:1 [E][Sealed][Frozen]'");
runTest("o = {foo:1, get foo(){return 3}}; Object.seal(o);", "'getter:function value:(3) keys:1 [E][Sealed][Frozen]'");
runTest("o = {foo:1, get foo(){return 3}}; Object.seal(o); Object.defineProperty(o, 'foo', {get(){return 5}})", null);
runTest("o = {foo:1, get foo(){return 3}}; Object.seal(o); Object.defineProperty(o, 'foo', {value:5})", null);
runTest("o = {foo:1, get foo(){return 3}}; Object.seal(o); o.__defineGetter__('foo', function(){return 5})");

// Computed properties with Accessor properties.
debug(""); debug("Computed + Accessor");
runTest("o = {['foo']:1, get foo(){return 2}}", "'getter:function value:(2) keys:1 [EC][Extensible]'");
runTest("o = {['foo']:1, get foo(){return 2}, set foo(x){}}", "'getter:function value:(2) setter:function keys:1 [EC][Extensible]'");
runTest("o = {get foo(){return 2}, ['foo']:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {get foo(){return 2}, set foo(x){}, ['foo']:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {['foo']:1, get foo(){return -1}, ['foo']:2}", "'value:2 keys:1 [ECW][Extensible]'");
runTest("o = {['foo']:1, get foo(){return 2}, ['bar']:3}", "'getter:function value:(2) keys:2 [EC][Extensible]'");
runTest("o = {['foo']:1, get foo(){return -1}, ['foo']:-1, get foo() {return 2}}", "'getter:function value:(2) keys:1 [EC][Extensible]'");

// Basic properties, Computed properties, and Accessor properties.
debug(""); debug("Basic + Computed + Accessor");
runTest("o = {foo:1, get foo(){return 2}, ['foo']:3}", "'value:3 keys:1 [ECW][Extensible]'");
runTest("o = {foo:1, ['foo']:3, get foo(){return 2}}", "'getter:function value:(2) keys:1 [EC][Extensible]'");
runTest("o = {foo:1, ['foo']:3, get foo(){return 2}, set foo(x){}}", "'getter:function value:(2) setter:function keys:1 [EC][Extensible]'");
runTest("o = {['foo']:3, get foo(){return 2}, foo:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {get foo(){return 2}, ['foo']:3, foo:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {get foo(){return 2}, ['foo']:3, get foo(){return 2}, foo:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {set foo(x){}, ['foo']:3, set foo(x){}, foo:1}", "'value:1 keys:1 [ECW][Extensible]'");
runTest("o = {set foo(x){}, foo:1, get foo(){return 2}, ['foo']:3}", "'value:3 keys:1 [ECW][Extensible]'");
runTest("o = {get foo(){return 2}, get foo(){return 2}, ['foo']:3, foo:1}", "'value:1 keys:1 [ECW][Extensible]'");

// __proto__ duplicates are not allowed.
function runProtoTestShouldThrow(test) {
    shouldThrow(test);
    shouldThrow("'use strict';" + test);
    shouldThrow("(function(){" + test + "})()");
}
function runProtoTestShouldNotThrow(test) {
    shouldNotThrow(test);
    shouldNotThrow("'use strict';" + test);
    shouldNotThrow("(function(){" + test + "})()");
}

debug(""); debug("Duplicate simple __proto__ attributes are not allowed");
runProtoTestShouldNotThrow("o = {__proto__:null}");
runProtoTestShouldNotThrow("({__proto__:null, ['__proto__']:{}})");
runProtoTestShouldNotThrow("o = {__proto__:null, ['__proto__']:{}}");
runProtoTestShouldNotThrow("o = {__proto__:null, get __proto__(){}}");
runProtoTestShouldNotThrow("var __proto__ = null; o = {__proto__:null, __proto__}");
runProtoTestShouldThrow("({__proto__:[], __proto__:{}})");
runProtoTestShouldThrow("o = {__proto__:null, '__proto__':{}}");
runProtoTestShouldThrow("o = {__proto__:[], __proto__:{}}");
runProtoTestShouldThrow("o = {'__proto__':{}, '__proto__':{}}");
runProtoTestShouldThrow("o = {a:1, __proto__:{}, b:2, ['c']:3, __proto__:{}, d:3}");
