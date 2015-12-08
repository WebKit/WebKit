description("This test checks String.prototype.toLocaleUpperCase().");

shouldBe("String.prototype.toLocaleUpperCase.length", "0");

// Check empty string optimization.
shouldBe("''.toLocaleUpperCase()", "''");

// Generic
shouldBe("String.prototype.toLocaleUpperCase.call({ toString() { return 'a' } })", "'A'");
shouldThrow("String.prototype.toLocaleUpperCase.call({ toString() { throw Error('1') } })", "'Error: 1'");
shouldThrow("String.prototype.toLocaleUpperCase.call(null)");
shouldThrow("String.prototype.toLocaleUpperCase.call(undefined)");

// Ignores non-object, non-string locale list.
shouldBe("'a'.toLocaleUpperCase(9)", "'A'");
// Handles array-like objects with holes.
shouldBe("'i'.toLocaleUpperCase({ length: 4, 1: 'az', 3: 'en' })", "'\\u0130'");
// Doesn't throw, but ignores private tags.
shouldBe("'a'.toLocaleUpperCase('x-some-thing')", "'A'");
// Throws on problems with length, get, or toString.
shouldThrow("'a'.toLocaleUpperCase(Object.create(null, { length: { get() { throw Error('a') } } }))", "'Error: a'");
shouldThrow("'a'.toLocaleUpperCase(Object.create(null, { length: { value: 1 }, 0: { get() { throw Error('b') } } }))", "'Error: b'");
shouldThrow("'a'.toLocaleUpperCase([ { toString() { throw Error('c') } } ])", "'Error: c'");
// Throws on bad tags.
shouldThrow("'a'.toLocaleUpperCase([ 5 ])", "'TypeError: locale value must be a string or object'");
shouldThrow("'a'.toLocaleUpperCase('')", "'RangeError: invalid language tag: '");
shouldThrow("'a'.toLocaleUpperCase('a')", "'RangeError: invalid language tag: a'");
shouldThrow("'a'.toLocaleUpperCase('abcdefghij')", "'RangeError: invalid language tag: abcdefghij'");
shouldThrow("'a'.toLocaleUpperCase('#$')", "'RangeError: invalid language tag: #$'");
shouldThrow("'a'.toLocaleUpperCase('en-@-abc')", "'RangeError: invalid language tag: en-@-abc'");
shouldThrow("'a'.toLocaleUpperCase('en-u')", "'RangeError: invalid language tag: en-u'");
shouldThrow("'a'.toLocaleUpperCase('en-u-kn-true-u-ko-true')", "'RangeError: invalid language tag: en-u-kn-true-u-ko-true'");
shouldThrow("'a'.toLocaleUpperCase('en-x')", "'RangeError: invalid language tag: en-x'");
shouldThrow("'a'.toLocaleUpperCase('en-*')", "'RangeError: invalid language tag: en-*'");
shouldThrow("'a'.toLocaleUpperCase('en-')", "'RangeError: invalid language tag: en-'");
shouldThrow("'a'.toLocaleUpperCase('en--US')", "'RangeError: invalid language tag: en--US'");

// Check ascii and accents.
shouldBe("'AbCdEfGhIjKlMnOpQRStuvWXyZ0123456789'.toLocaleUpperCase()", "'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'");
shouldBe("'àéîöœøūñ'.toLocaleUpperCase()", "'ÀÉÎÖŒØŪÑ'");

// Check non-special case for i.
shouldBe("'i'.toLocaleUpperCase('und')", "'I'");

// Check for special casing of Azeri.
shouldBe("'\\u0130'.toLocaleUpperCase('az')", "'\\u0130'");
shouldBe("'I'.toLocaleUpperCase('az')", "'I'");
shouldBe("'i'.toLocaleUpperCase('az')", "'\\u0130'");
shouldBe("'\\u0131'.toLocaleUpperCase('az')", "'I'");

// Check for special casing of Lithuanian.
shouldBe("'I\\u0307'.toLocaleUpperCase('lt')", "'I\\u0307'");
shouldBe("'J\\u0307'.toLocaleUpperCase('lt')", "'J\\u0307'");
// Code points with Soft_Dotted property (Unicode 5.1, PropList.txt)
var softDotted = [
    "\\u0069", "\\u006A",  // LATIN SMALL LETTER I..LATIN SMALL LETTER J
    "\\u012F",             // LATIN SMALL LETTER I WITH OGONEK
    "\\u0249",             // LATIN SMALL LETTER J WITH STROKE
    "\\u0268",             // LATIN SMALL LETTER I WITH STROKE
    "\\u029D",             // LATIN SMALL LETTER J WITH CROSSED-TAIL
    "\\u02B2",             // MODIFIER LETTER SMALL J
    "\\u03F3",             // GREEK LETTER YOT
    "\\u0456",             // CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I
    "\\u0458",             // CYRILLIC SMALL LETTER JE
    "\\u1D62",             // LATIN SUBSCRIPT SMALL LETTER I
    "\\u1D96",             // LATIN SMALL LETTER I WITH RETROFLEX HOOK
    "\\u1DA4",             // MODIFIER LETTER SMALL I WITH STROKE
    "\\u1DA8",             // MODIFIER LETTER SMALL J WITH CROSSED-TAIL
    "\\u1E2D",             // LATIN SMALL LETTER I WITH TILDE BELOW
    "\\u1ECB",             // LATIN SMALL LETTER I WITH DOT BELOW
    "\\u2071",             // SUPERSCRIPT LATIN SMALL LETTER I
    "\\u2148", "\\u2149",  // DOUBLE-STRUCK ITALIC SMALL I..DOUBLE-STRUCK ITALIC SMALL J
    "\\u2C7C",             // LATIN SUBSCRIPT SMALL LETTER J
    "\\uD835\\uDC22", "\\uD835\\uDC23",   // MATHEMATICAL BOLD SMALL I..MATHEMATICAL BOLD SMALL J
    "\\uD835\\uDC56", "\\uD835\\uDC57",   // MATHEMATICAL ITALIC SMALL I..MATHEMATICAL ITALIC SMALL J
    "\\uD835\\uDC8A", "\\uD835\\uDC8B",   // MATHEMATICAL BOLD ITALIC SMALL I..MATHEMATICAL BOLD ITALIC SMALL J
    "\\uD835\\uDCBE", "\\uD835\\uDCBF",   // MATHEMATICAL SCRIPT SMALL I..MATHEMATICAL SCRIPT SMALL J
    "\\uD835\\uDCF2", "\\uD835\\uDCF3",   // MATHEMATICAL BOLD SCRIPT SMALL I..MATHEMATICAL BOLD SCRIPT SMALL J
    "\\uD835\\uDD26", "\\uD835\\uDD27",   // MATHEMATICAL FRAKTUR SMALL I..MATHEMATICAL FRAKTUR SMALL J
    "\\uD835\\uDD5A", "\\uD835\\uDD5B",   // MATHEMATICAL DOUBLE-STRUCK SMALL I..MATHEMATICAL DOUBLE-STRUCK SMALL J
    "\\uD835\\uDD8E", "\\uD835\\uDD8F",   // MATHEMATICAL BOLD FRAKTUR SMALL I..MATHEMATICAL BOLD FRAKTUR SMALL J
    "\\uD835\\uDDC2", "\\uD835\\uDDC3",   // MATHEMATICAL SANS-SERIF SMALL I..MATHEMATICAL SANS-SERIF SMALL J
    "\\uD835\\uDDF6", "\\uD835\\uDDF7",   // MATHEMATICAL SANS-SERIF BOLD SMALL I..MATHEMATICAL SANS-SERIF BOLD SMALL J
    "\\uD835\\uDE2A", "\\uD835\\uDE2B",   // MATHEMATICAL SANS-SERIF ITALIC SMALL I..MATHEMATICAL SANS-SERIF ITALIC SMALL J
    "\\uD835\\uDE5E", "\\uD835\\uDE5F",   // MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL I..MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL J
    "\\uD835\\uDE92", "\\uD835\\uDE93",   // MATHEMATICAL MONOSPACE SMALL I..MATHEMATICAL MONOSPACE SMALL J
];
// COMBINING DOT ABOVE (U+0307) removed when preceded by Soft_Dotted.
for (var i = 0; i < softDotted.length; ++i) {
    shouldBe("'" + softDotted[i] + "\\u0307'.toLocaleUpperCase('lt')", "'" + softDotted[i] + "'.toLocaleUpperCase('und')");
    shouldBe("'" + softDotted[i] + "\\u0323\\u0307'.toLocaleUpperCase('lt')", "'" + softDotted[i] + "'.toLocaleUpperCase('und') + '\\u0323'");
    shouldBe("'" + softDotted[i] + "\\uD800\\uDDFD\\u0307'.toLocaleUpperCase('lt')", "'" + softDotted[i] + "'.toLocaleUpperCase('und') + '\\uD800\\uDDFD'");
}

// Check for special casing of Turkish.
shouldBe("'\\u0130'.toLocaleUpperCase('tr')", "'\\u0130'");
shouldBe("'I'.toLocaleUpperCase('tr')", "'I'");
shouldBe("'i'.toLocaleUpperCase('tr')", "'\\u0130'");
shouldBe("'\\u0131'.toLocaleUpperCase('tr')", "'I'");
