//@ requireOptions("--useExecutableAllocationFuzz=true", "--fireExecutableAllocationFuzzAtOrAfter=1")

// Regression test for optimizeAlternative swapping an inverted character class
// with a fixed character in Char8 mode. [^a] is BMP-only in its class data, but
// being inverted it can match non-BMP characters (variable width in UTF-16).
// When JIT allocation fails and falls back to bytecode with the already-swapped
// pattern, executing against a Char16 string can cause surrogate pair misreads.

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// Force Char8 compilation first (Latin1 string), then test with Char16 string
// containing a non-BMP character. The non-BMP character \u{10000} is encoded as
// the surrogate pair \uD800\uDC00 in UTF-16.

var re = /[^a]bcd/u;

// Char8 path: Latin1 string triggers Char8 JIT compilation (which does the swap)
var result8 = re.exec("xbcd");
shouldBe(result8[0], "xbcd", "Char8 match");

// Char16 path: String with non-BMP character forces Char16 execution.
// \u{10000} is not 'a', so [^a] should match it, followed by "bcd".
var result16 = re.exec("\u{10000}bcd");
shouldBe(result16 !== null, true, "Char16 match should not be null");
shouldBe(result16[0], "\u{10000}bcd", "Char16 match with non-BMP character");

// Additional test: ensure [^a] does NOT match 'a'
var resultNoMatch = re.exec("abcd");
shouldBe(resultNoMatch, null, "should not match 'a' followed by bcd");
