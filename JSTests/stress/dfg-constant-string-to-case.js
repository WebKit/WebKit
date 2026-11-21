function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

function lower(string) { return string.toLowerCase(); }
function upper(string) { return string.toUpperCase(); }

const table = { "x-request-id": 1, "X-REQUEST-ID": 2 };

// A 16-bit string whose characters are all ASCII.
const wide = "\u3042Ab-Z".slice(1);

function inlinedKey() { return table[lower("X-Request-Id")] * 10 + table[upper("X-Request-Id")]; }
noInline(inlinedKey);

function ascii() {
    shouldBe("".toLowerCase(), "");
    shouldBe("".toUpperCase(), "");
    shouldBe("A".toLowerCase(), "a");
    shouldBe("a".toUpperCase(), "A");
    shouldBe("already-lower".toLowerCase(), "already-lower");
    shouldBe("ALREADY-UPPER".toUpperCase(), "ALREADY-UPPER");
    shouldBe("@AZ[`az{\0\x7f".toLowerCase(), "@az[`az{\0\x7f");
    shouldBe("@AZ[`az{\0\x7f".toUpperCase(), "@AZ[`AZ{\0\x7f");
    shouldBe(wide.toLowerCase(), "ab-z");
    shouldBe(wide.toUpperCase(), "AB-Z");
}
noInline(ascii);

function concatenated() { return lower("Con" + "Tent") + upper("Ty" + "pe"); }
noInline(concatenated);

function chained() { return upper(lower("Content-Type")); }
noInline(chained);

function nonASCII() {
    shouldBe("\u0130Stra\u00dfe\u00c4".toLowerCase(), "i\u0307stra\u00dfe\u00e4");
    shouldBe("stra\u00dfe\u00e4".toUpperCase(), "STRASSE\u00c4");
    shouldBe("\u03a3\u03a3".toLowerCase(), "\u03c3\u03c2");
    shouldBe("\xb5".toUpperCase(), "\u039c");
    shouldBe("A\u212a".toLowerCase(), "ak");
}
noInline(nonASCII);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(inlinedKey(), 12);
    ascii();
    shouldBe(concatenated(), "contentTYPE");
    shouldBe(chained(), "CONTENT-TYPE");
    nonASCII();
}

// The compiled code must notice when the methods are replaced.
String.prototype.toLowerCase = function () { return "replaced lower"; };
shouldBe(chained(), "REPLACED LOWER");
String.prototype.toUpperCase = function () { return "replaced upper"; };
shouldBe(chained(), "replaced upper");
