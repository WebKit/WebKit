description("This test makes sure we don't hang when setting cyclic prototype values: http://bugs.webkit.org/show_bug.cgi?id=6985")

var o1 = { p1: 1 };
var o2 = { p2: 2 };
o2.__proto__ = o1;
var o3 = { p3: 3 };
o3.__proto__ = o2;

// Try to create a cyclical prototype chain.
shouldThrow("o1.__proto__ = o3;");

// This changes behaviour, since __proto__ is an accessor on Object.prototype.
o1.__proto__ = null;

shouldBeFalse("({}).hasOwnProperty.call(o1, '__proto__')");
o1.__proto__ = o3;
shouldBeTrue("({}).hasOwnProperty.call(o1, '__proto__')");
shouldBe("Object.getPrototypeOf(o1)", "null");
