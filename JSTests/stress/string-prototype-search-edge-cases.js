// Edge case coverage for the C++ implementation of String.prototype.search.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

function shouldThrow(func, errorType) {
    var threw = false;
    try {
        func();
    } catch (e) {
        threw = e instanceof errorType;
    }
    if (!threw)
        throw new Error("expected to throw " + errorType.name);
}

// Basic behavior.
shouldBe("abc".search(/b/), 1);
shouldBe("abc".search(/x/), -1);
shouldBe("abc".search(/a/), 0);
shouldBe("".search(/x/), -1);
shouldBe("".search(/(?:)/), 0);
shouldBe("abc".search(new RegExp("c")), 2);

// No argument / undefined / null arguments.
shouldBe("abc".search(), 0); // RegExpCreate(undefined) -> /(?:)/
shouldBe("abc".search(undefined), 0);
shouldBe("abcnull".search(null), 3); // pattern is the string "null"

// Primitive patterns are coerced through RegExpCreate.
shouldBe("a1b2".search("[0-9]"), 1);
shouldBe("a.c".search("."), 0); // "." is a regexp wildcard
shouldBe("abc123".search(123), 3);
shouldBe("abctrue".search(true), 3);

// |this| coercion.
shouldBe(String.prototype.search.call(12345, /3/), 2);
shouldBe(String.prototype.search.call(true, /ru/), 1);
shouldThrow(() => String.prototype.search.call(null, /a/), TypeError);
shouldThrow(() => String.prototype.search.call(undefined, /a/), TypeError);
shouldThrow(() => "abc".search.call(undefined), TypeError);

// |this| with a custom toString.
shouldBe(String.prototype.search.call({ toString() { return "xyz"; } }, /y/), 1);

// Global flag and lastIndex are ignored: search always starts from index 0 and
// restores lastIndex.
{
    var re = /b/g;
    re.lastIndex = 2;
    shouldBe("abcabc".search(re), 1);
    shouldBe(re.lastIndex, 2);
}
{
    var re = /b/y;
    shouldBe("ba".search(re), 0);
    shouldBe("ab".search(re), -1);
    shouldBe(re.lastIndex, 0);
}

// Non-writable lastIndex: resetting to 0 throws only when it is not already 0.
{
    var re = /b/;
    re.lastIndex = 3;
    Object.defineProperty(re, "lastIndex", { writable: false });
    shouldThrow(() => "abc".search(re), TypeError);

    var reZero = /b/;
    Object.defineProperty(reZero, "lastIndex", { writable: false });
    shouldBe("abc".search(reZero), 1);
}

// Custom @@search on a plain object.
shouldBe("abc".search({ [Symbol.search]() { return 42; } }), 42);

// @@search receives the original |this| value, not a string.
{
    var receivedArg;
    var searcher = { [Symbol.search](arg) { receivedArg = arg; return 7; } };
    var thisObject = { toString() { return "should-not-be-called-by-search"; } };
    shouldBe(String.prototype.search.call(thisObject, searcher), 7);
    shouldBe(receivedArg, thisObject);
}

// undefined / null @@search on an object falls back to RegExpCreate(ToString(object)).
shouldBe("xbcx".search({ [Symbol.search]: undefined, toString() { return "b"; } }), 1);
shouldBe("xbcx".search({ [Symbol.search]: null, toString() { return "c"; } }), 2);

// Non-callable @@search throws.
shouldThrow(() => "abc".search({ [Symbol.search]: 1 }), TypeError);
shouldThrow(() => "abc".search({ [Symbol.search]: "x" }), TypeError);

// Throwing @@search getter propagates.
shouldThrow(() => "abc".search({ get [Symbol.search]() { throw new TypeError("getter"); } }), TypeError);

// RegExp subclasses without overrides behave like plain regexps.
{
    class MyRegExp extends RegExp { }
    shouldBe("a1b2".search(new MyRegExp("[0-9]")), 1);
}

// RegExp subclasses overriding @@search are observed.
{
    class MyRegExp2 extends RegExp {
        [Symbol.search](str) { return "custom:" + str; }
    }
    shouldBe("a1b2".search(new MyRegExp2("[0-9]")), "custom:a1b2");
}

// RegExp subclasses overriding exec are observed through RegExp.prototype[@@search].
{
    class MyRegExp3 extends RegExp {
        exec() { return { index: 99 }; }
    }
    shouldBe("a1b2".search(new MyRegExp3("[0-9]")), 99);
}

// A fake regexp-like object with only @@search.
{
    var fake = {
        [Symbol.search](str) { return str.length; }
    };
    shouldBe("hello".search(fake), 5);
}

// Unicode handling: the result is a code unit index.
shouldBe("\u{20BB7}a".search(/a/), 2);
shouldBe("\u{20BB7}a".search(/\u{20BB7}/u), 0);
shouldBe("あいう".search(/う/), 2);

// Rope strings.
{
    var left = "a".repeat(10);
    var right = "1" + "b".repeat(10);
    shouldBe((left + right).search(/[0-9]/), 10);
}

// Captures and legacy RegExp statics still work through search.
{
    "abc1def".search(/([0-9])/);
    shouldBe(RegExp.$1, "1");
    shouldBe(RegExp.lastMatch, "1");
}

// Function properties.
shouldBe(String.prototype.search.length, 1);
shouldBe(String.prototype.search.name, "search");
shouldBe(typeof RegExp.prototype[Symbol.search], "function");

// Property attributes stay the same as before (writable, non-enumerable, configurable).
{
    var descriptor = Object.getOwnPropertyDescriptor(String.prototype, "search");
    shouldBe(descriptor.writable, true);
    shouldBe(descriptor.enumerable, false);
    shouldBe(descriptor.configurable, true);
}
