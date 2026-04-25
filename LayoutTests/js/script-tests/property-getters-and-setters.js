description(
"This performs a number of different tests on JavaScript getters and setters."
);

debug("the get set object declaration syntax");
var o1 = { 'a':7, get b() { return this.a + 1 }, set b(x) { this.a = x } }
shouldBe("o1.b", "8");
o1.b = 10;
shouldBe("o1.b", "11");

debug("__defineGetter__ and __defineSetter__");
var o2 = new Object()
o2.a = 7;
o2.__defineGetter__('b', function getB() { return this.a + 1} )
o2.__defineSetter__('b', function setB(x) { this.a = x})
shouldBe("o2.b", "8");
o2.b = 10;
shouldBe("o2.b", "11");

debug("Setting a value without having a setter");
var o3 = { get x() { return 42; } }
shouldBe("o3.x = 10; o3.x", "42");

debug("Getting a value without having a getter");
var o4 = { set x(y) { }}
shouldBeUndefined("o4.x");

debug("__lookupGetter__ and __lookupSetter__");
var o4 = new Object()
function getB() { return this.a }
function setB(x) { this.a = x }
o4.__defineGetter__('b', getB)
o4.__defineSetter__('b', setB)

shouldBe("o4.__lookupGetter__('b')", "getB");
shouldBe("o4.__lookupSetter__('b')", "setB");

debug("__defineGetter__ and __defineSetter__ with various invalid arguments");
var numExceptions = 0;
var o5 = new Object();
shouldThrow("o5.__defineSetter__('a', null)");
shouldThrow("o5.__defineSetter__('a', o5)");
shouldThrow("o5.__defineGetter__('a', null)");
shouldThrow("o5.__defineGetter__('a', o5)");

debug("setters and getters with exceptions");
var o6 = { get x() { throw 'Exception in get'}, set x(f) { throw 'Exception in set'}}
var x = 0;
var numExceptions = 0;
shouldThrow("x = o6.x");
shouldBe("x", "0");
shouldThrow("o6.x = 42");

debug("Defining a setter should also define a getter for the same property which returns undefined. Thus, a getter defined on the prototype should not be called.");
o7 = { 'a':7, set x(b) { this.a = b; }}
o7.prototype = { get x() { return 42; }}
shouldBeUndefined("o7.x")

debug("If an object has a property and its prototype has a setter function for that property, then setting the property should set the property directly and not call the setter function.");
var o8 = new Object()
o8.numSets = 0;
o8.x = 10;
o8.__proto__.__defineSetter__('x', function() { this.numSets++; })
o8.x = 20;
shouldBe("o8.numSets", "0");

({getter:"foo", b:"bar"});
testObj=({get getter(){return 'getter was called.'}, b: 'bar'})
shouldBe("typeof testObj.getter", "'string'");

debug("the get set with string property name");
var o9 = { 'a':7, get 'b'() { return this.a + 1 }, set 'b'(x) { this.a = x } }
shouldBe("o9.b", "8");
o9.b = 10;
shouldBe("o9.b", "11");

debug("the get set with numeric property name");
var o10 = { 'a':7, get 42() { return this.a + 1 }, set 42(x) { this.a = x } }
shouldBe("o10[42]", "8");
o10[42] = 10;
shouldBe("o10[42]", "11");

debug("Defining getter only and accessing __lookupSetter__ should not crash");
var o11 = new Object()
function getB() { return this.a }
o11.__defineGetter__('b', getB)

shouldBe("o11.__lookupSetter__('b')", "void 0");

debug("Defining setter only and accessing __lookupGetter__ should not crash");
var o12 = new Object()
function setB(x) { this.a = x }
o12.__defineSetter__('b', setB)

shouldBe("o12.__lookupGetter__('b')", "void 0");

debug("When undefined, accessing __lookupGetter__ and __lookupSetter__ should not crash");
var o13 = new Object()

shouldBe("o13.__lookupGetter__('b')", "void 0");
shouldBe("o13.__lookupSetter__('b')", "void 0");

debug("__defineGetter__ and __defineSetter__ should throw exceptions when acting on sealed objects");
var o14 = {a:14};
Object.seal(o14);
shouldThrow("o14.__defineGetter__('a', function(){})");
shouldThrow("o14.__defineGetter__('b', function(){})");
shouldThrow("o14.__defineSetter__('a', function(){})");
shouldThrow("o14.__defineSetter__('b', function(){})");

debug("__defineGetter__ and __defineSetter__ should throw exceptions when acting on frozen objects");
var o15 = {a:15};
Object.freeze(o15);
shouldThrow("o15.__defineGetter__('a', function(){})");
shouldThrow("o15.__defineGetter__('b', function(){})");
shouldThrow("o15.__defineSetter__('a', function(){})");
shouldThrow("o15.__defineSetter__('b', function(){})");

debug("__defineGetter__ and __defineSetter__ should throw exceptions when acting on unconfigurable properties");
var o16 = {a:16};
Object.defineProperty(o16, "b", {value: 16, configurable: false});
shouldNotThrow("o16.__defineGetter__('a', function(){})");
shouldNotThrow("o16.__defineSetter__('a', function(){})");
shouldThrow("o16.__defineSetter__('b', function(){})");
shouldThrow("o16.__defineSetter__('b', function(){})");

debug("__lookupGetter__ can be interrupted by a proxy throwing an exception");
var getter17 = () => { return "WebKit!"}
var o17 = Object.defineProperty(new Object, 'property', { get: getter17 });
shouldBeEqualToString("o17.property", "WebKit!");
shouldBe("o17.__lookupGetter__('property')", "getter17");
shouldBe("o17.__lookupSetter__('property')", "undefined");

var o17Exception = { toString: () => { return "o17 Proxy raised exception"; } };
var o17Proxy = new Proxy(o17, { getOwnPropertyDescriptor: () => { throw o17Exception; } });
shouldThrow("o17Proxy.__lookupGetter__('property')", o17Exception);
shouldThrow("o17Proxy.__lookupSetter__('property')", o17Exception);
shouldBeEqualToString("o17Proxy.property", "WebKit!");
shouldBeEqualToString("o17.property", "WebKit!");
shouldBe("o17.__lookupGetter__('property')", "getter17");

debug("__lookupSetter__ can be interrupted by a proxy throwing an exception");
var setter18 = function (newValue) { return this.value = newValue; }
var o18 = Object.defineProperty(new Object, 'property', { set: setter18 });
shouldBe("o18.property = 5", "5");
shouldBe("o18.property", "undefined");
shouldBe("o18.value", "5");
shouldBe("o18.__lookupGetter__('property')", "undefined");
shouldBe("o18.__lookupSetter__('property')", "setter18");

var o18Exception = { toString: () => { return "o18 Proxy raised exception"; } };
var o18Proxy = new Proxy(o18, { getOwnPropertyDescriptor: () => { throw o18Exception; } });
shouldThrow("o18Proxy.__lookupGetter__('property')", o18Exception);
shouldThrow("o18Proxy.__lookupSetter__('property')", o18Exception);
shouldThrow("o18Proxy.property = 'JavaScriptCore!'", o18Exception);
shouldBe("o18Proxy.property", "undefined");
shouldBe("o18Proxy.value", "5");
shouldBeEqualToString("o18.property = 'JavaScriptCore!'", "JavaScriptCore!");
shouldBe("o18.property", "undefined");
shouldBeEqualToString("o18.value", "JavaScriptCore!");
shouldBe("o18.__lookupGetter__('property')", "undefined");
shouldBe("o18.__lookupSetter__('property')", "setter18");
