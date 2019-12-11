description("This test checks the String.prototype.padEnd.");

shouldBe('String.prototype.padEnd.length', '1');
shouldBeEqualToString('String.prototype.padEnd.name', 'padEnd');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padEnd").configurable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padEnd").enumerable', 'false');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padEnd").writable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padEnd").get', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padEnd").set', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padEnd").value', 'String.prototype.padEnd');

shouldBe("'foo'.padEnd()", "'foo'");
shouldBe("'foo'.padEnd(+0)", "'foo'");
shouldBe("'foo'.padEnd(-0)", "'foo'");
shouldBe("'foo'.padEnd(1)", "'foo'");
shouldBe("'foo'.padEnd(2)", "'foo'");
shouldBe("'foo'.padEnd(-2)", "'foo'");
shouldBe("'foo'.padEnd(10)",              "'foo       '");
shouldBe("'foo'.padEnd(10, undefined)",   "'foo       '");
shouldBe("'foo'.padEnd(10, 'x')",         "'fooxxxxxxx'");
shouldBe("'foo'.padEnd(10.5, 'z')",       "'foozzzzzzz'");
shouldBe("'foo'.padEnd(10, 'bar')",       "'foobarbarb'");
shouldBe("'foo'.padEnd(10, '123456789')", "'foo1234567'");
shouldBe("'foo'.padEnd(999, '')", "'foo'");
shouldBe("''.padEnd(1, '')", "''");
shouldBe("''.padEnd(2, 'bar')", "'ba'");
shouldBe("'x'.padEnd(2, 'bar')", "'xb'");
shouldBe("'xx'.padEnd(2, 'bar')", "'xx'");
shouldBe("'xx'.padEnd(Math.PI, 'bar')", "'xxb'");

// Coerce length (ToLength).
shouldBe("''.padEnd(true, 'ABC')", "'A'");
shouldBe("''.padEnd(false, 'ABC')", "''");
shouldBe("''.padEnd(null, 'ABC')", "''");
shouldBe("''.padEnd({}, 'ABC')", "''");
shouldBe("''.padEnd(NaN, 'ABC')", "''");

// Coerce fillString (ToString).
shouldBe("'ABC'.padEnd(10, true)",  "'ABCtruetru'");
shouldBe("'ABC'.padEnd(10, false)", "'ABCfalsefa'");
shouldBe("'ABC'.padEnd(10, null)",  "'ABCnullnul'");
shouldBe("'ABC'.padEnd(10, {})",    "'ABC[object'");
shouldBe("'ABC'.padEnd(10, NaN)",   "'ABCNaNNaNN'");

// Check out of memory errors.
shouldNotThrow('"x".padEnd(Infinity, "")'); // Empty string filler is fine.
shouldThrow('"x".padEnd(Infinity, "x")', "'Error: Out of memory'");
shouldThrow('"x".padEnd(0x80000000, "x")', "'Error: Out of memory'");
shouldThrow('"x".padEnd(0xFFFFFFFF, "x")', "'Error: Out of memory'");

// Check side-effects.
let sideEffects = "";
let thisObject = new String("foo bar");
let lengthObject = new Number(10);
let fillObject = new String("X");

sideEffects = "";
thisObject.toString = function() { sideEffects += "A"; return this; };
lengthObject.valueOf = function() { sideEffects += "B"; return this; };
fillObject.toString = function() { sideEffects += "C"; return this; };
shouldBeEqualToString("String.prototype.padEnd.call(thisObject, lengthObject, fillObject)", "foo barXXX");
shouldBeEqualToString("sideEffects", "ABC");

sideEffects = "";
thisObject.toString = function() { throw "ERROR"; };
lengthObject.valueOf = function() { sideEffects += "B"; return this; };
fillObject.toString = function() { sideEffects += "C"; return this; };
shouldThrow("String.prototype.padEnd.call(thisObject, lengthObject, fillObject)", "'ERROR'");
shouldBeEqualToString("sideEffects", "");

sideEffects = "";
thisObject.toString = function() { sideEffects += "A"; return this; };
lengthObject.valueOf = function() { throw "ERROR"; };
fillObject.toString = function() { sideEffects += "C"; return this; };
shouldThrow("String.prototype.padEnd.call(thisObject, lengthObject, fillObject)", "'ERROR'");
shouldBeEqualToString("sideEffects", "A");

sideEffects = "";
thisObject.toString = function() { sideEffects += "A"; return this; };
lengthObject.valueOf = function() { sideEffects += "B"; return this; };
fillObject.toString = function() { throw "ERROR"; return this; };
shouldThrow("String.prototype.padEnd.call(thisObject, lengthObject, fillObject)", "'ERROR'");
shouldBeEqualToString("sideEffects", "AB");
