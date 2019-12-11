description("This test checks the String.prototype.padStart.");

shouldBe('String.prototype.padStart.length', '1');
shouldBeEqualToString('String.prototype.padStart.name', 'padStart');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padStart").configurable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padStart").enumerable', 'false');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padStart").writable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padStart").get', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padStart").set', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "padStart").value', 'String.prototype.padStart');

shouldBe("'foo'.padStart()", "'foo'");
shouldBe("'foo'.padStart(+0)", "'foo'");
shouldBe("'foo'.padStart(-0)", "'foo'");
shouldBe("'foo'.padStart(1)", "'foo'");
shouldBe("'foo'.padStart(2)", "'foo'");
shouldBe("'foo'.padStart(-2)", "'foo'");
shouldBe("'foo'.padStart(10)",              "'       foo'");
shouldBe("'foo'.padStart(10, undefined)",   "'       foo'");
shouldBe("'foo'.padStart(10, 'x')",         "'xxxxxxxfoo'");
shouldBe("'foo'.padStart(10.5, 'z')",       "'zzzzzzzfoo'");
shouldBe("'foo'.padStart(10, 'bar')",       "'barbarbfoo'");
shouldBe("'foo'.padStart(10, '123456789')", "'1234567foo'");
shouldBe("'foo'.padStart(999, '')", "'foo'");
shouldBe("''.padStart(1, '')", "''");
shouldBe("''.padStart(2, 'bar')", "'ba'");
shouldBe("'x'.padStart(2, 'bar')", "'bx'");
shouldBe("'xx'.padStart(2, 'bar')", "'xx'");
shouldBe("'xx'.padStart(Math.PI, 'bar')", "'bxx'");

// Coerce length (ToLength).
shouldBe("''.padStart(true, 'ABC')", "'A'");
shouldBe("''.padStart(false, 'ABC')", "''");
shouldBe("''.padStart(null, 'ABC')", "''");
shouldBe("''.padStart({}, 'ABC')", "''");
shouldBe("''.padStart(NaN, 'ABC')", "''");

// Coerce fillString (ToString).
shouldBe("'ABC'.padStart(10, true)",  "'truetruABC'");
shouldBe("'ABC'.padStart(10, false)", "'falsefaABC'");
shouldBe("'ABC'.padStart(10, null)",  "'nullnulABC'");
shouldBe("'ABC'.padStart(10, {})",    "'[objectABC'");
shouldBe("'ABC'.padStart(10, NaN)",   "'NaNNaNNABC'");

// Check out of memory errors.
shouldNotThrow('"x".padStart(Infinity, "")'); // Empty string filler is fine.
shouldThrow('"x".padStart(Infinity, "x")', "'Error: Out of memory'");
shouldThrow('"x".padStart(0x80000000, "x")', "'Error: Out of memory'");
shouldThrow('"x".padStart(0xFFFFFFFF, "x")', "'Error: Out of memory'");

// Check side-effects.
let sideEffects = "";
let thisObject = new String("foo bar");
let lengthObject = new Number(10);
let fillObject = new String("X");

sideEffects = "";
thisObject.toString = function() { sideEffects += "A"; return this; };
lengthObject.valueOf = function() { sideEffects += "B"; return this; };
fillObject.toString = function() { sideEffects += "C"; return this; };
shouldBeEqualToString("String.prototype.padStart.call(thisObject, lengthObject, fillObject)", "XXXfoo bar");
shouldBeEqualToString("sideEffects", "ABC");

sideEffects = "";
thisObject.toString = function() { throw "ERROR"; };
lengthObject.valueOf = function() { sideEffects += "B"; return this; };
fillObject.toString = function() { sideEffects += "C"; return this; };
shouldThrow("String.prototype.padStart.call(thisObject, lengthObject, fillObject)", "'ERROR'");
shouldBeEqualToString("sideEffects", "");

sideEffects = "";
thisObject.toString = function() { sideEffects += "A"; return this; };
lengthObject.valueOf = function() { throw "ERROR"; };
fillObject.toString = function() { sideEffects += "C"; return this; };
shouldThrow("String.prototype.padStart.call(thisObject, lengthObject, fillObject)", "'ERROR'");
shouldBeEqualToString("sideEffects", "A");

sideEffects = "";
thisObject.toString = function() { sideEffects += "A"; return this; };
lengthObject.valueOf = function() { sideEffects += "B"; return this; };
fillObject.toString = function() { throw "ERROR"; return this; };
shouldThrow("String.prototype.padStart.call(thisObject, lengthObject, fillObject)", "'ERROR'");
shouldBeEqualToString("sideEffects", "AB");
