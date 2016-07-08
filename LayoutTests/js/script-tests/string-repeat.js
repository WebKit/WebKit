description("This test checks String.prototype.repeat.");

shouldBe('String.prototype.repeat.length', '1');
shouldBeEqualToString('String.prototype.repeat.name', 'repeat');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "repeat").configurable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "repeat").enumerable', 'false');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "repeat").writable', 'true');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "repeat").get', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "repeat").set', 'undefined');
shouldBe('Object.getOwnPropertyDescriptor(String.prototype, "repeat").value', 'String.prototype.repeat');

shouldBe("'foo bar'.repeat(+0)", "''");
shouldBe("'foo bar'.repeat(-0)", "''");
shouldBe("'foo bar'.repeat(1)", "'foo bar'");
shouldBe("'foo bar'.repeat(2)", "'foo barfoo bar'");
shouldBe("'フーバー'.repeat(0)", "''");
shouldBe("'フーバー'.repeat(1)", "'フーバー'");
shouldBe("'フーバー'.repeat(2)", "'フーバーフーバー'");
shouldBe("'foo barfoo bar'.repeat(2)", "'foo barfoo barfoo barfoo bar'");
shouldBe("'foo barfoo bar'.repeat(2.2)", "'foo barfoo barfoo barfoo bar'");
shouldBe("'foo barfoo bar'.repeat(2.8)", "'foo barfoo barfoo barfoo bar'");
shouldBe("'foo'.repeat(3.1)", "'foofoofoo'");
shouldBe("'foo'.repeat('2')", "'foofoo'");
shouldBe("'foo'.repeat(NaN)", "''");
shouldBe("'foo'.repeat(null)", "''");
shouldBe("'foo'.repeat(true)", "'foo'");
shouldBe("'foo'.repeat(false)", "''");
shouldBe("'foo'.repeat(undefined)", "''");
shouldBe("'foo'.repeat()", "''");
shouldBe("'f'.repeat(0)", "''");
shouldBe("'f'.repeat(1)", "'f'");
shouldBe("'f'.repeat(10)", "'ffffffffff'");
shouldBe("'フ'.repeat(0)", "''");
shouldBe("'フ'.repeat(1)", "'フ'");
shouldBe("'フ'.repeat(2)", "'フフ'");

// Repeat empty strings.
shouldBe("''.repeat(1000)", "''");
shouldBe("''.repeat(0xFFFFFFFF)", "''");
shouldBe("''.repeat(0xFFFFFFFF + 1)", "''");

// Check range errors.
shouldThrow("'x'.repeat(-1)", "'RangeError: String.prototype.repeat argument must be greater than or equal to 0 and not be Infinity'");
shouldThrow("'x'.repeat(Infinity)", "'RangeError: String.prototype.repeat argument must be greater than or equal to 0 and not be Infinity'");
shouldThrow("'x'.repeat(-Infinity)", "'RangeError: String.prototype.repeat argument must be greater than or equal to 0 and not be Infinity'");
shouldThrow("'foo bar'.repeat(-1)", "'RangeError: String.prototype.repeat argument must be greater than or equal to 0 and not be Infinity'");
shouldThrow("'foo bar'.repeat(Infinity)", "'RangeError: String.prototype.repeat argument must be greater than or equal to 0 and not be Infinity'");
shouldThrow("'foo bar'.repeat(-Infinity)", "'RangeError: String.prototype.repeat argument must be greater than or equal to 0 and not be Infinity'");

// Check out of memory errors.
shouldThrow("'f'.repeat(0xFFFFFFFF)", "'Error: Out of memory'");
shouldThrow("'f'.repeat(0xFFFFFFFF + 1)", "'Error: Out of memory'");
shouldThrow("'foo'.repeat(0xFFFFFFFFF)", "'Error: Out of memory'");
shouldThrow("'foo'.repeat(0xFFFFFFFFF + 1)", "'Error: Out of memory'");
shouldThrow("'foo bar'.repeat(0xFFFFFFFF)", "'Error: Out of memory'");
shouldThrow("'foo bar'.repeat(0xFFFFFFFF + 1)", "'Error: Out of memory'");

var sideEffect, stringRepeated, count;
function checkSideEffects(str) {
    // Check side effects in repeat.
    sideEffect = "";
    stringRepeated = new String(str);
    stringRepeated.toString = function() {
        sideEffect += "A";
        return this;
    }
    count = new Number(2);
    count.valueOf = function() {
        sideEffect += "B";
        return this;
    }
    // Calling stringRepeated.repeat implicitly calls stringRepeated.toString(),
    // and count.valueOf(), in that respective order.
    shouldBe("stringRepeated.repeat(count)", "'" + str + str + "'");
    shouldBe("sideEffect == 'AB'", "true");

    // If stringRepeated.toString() throws an exception count.valueOf() is not called.
    stringRepeated.toString = function() {
        throw "error";
    }
    sideEffect = "";
    shouldThrow("stringRepeated.repeat(count)", "'error'");
    shouldBe("sideEffect == ''", "true");

    // If count throws an exception stringRepeated.toString() was called.
    stringRepeated.toString = function() {
        sideEffect += "A";
        return this;
    }
    count.valueOf = function() {
        throw "error";
    }
    sideEffect = "";
    shouldThrow("stringRepeated.repeat(count)", "'error'");
    shouldBe("sideEffect == 'A'", "true");
}

// Fast path for single character string.
checkSideEffects("x");

// Slow path for any other string.
checkSideEffects("foo bar");
