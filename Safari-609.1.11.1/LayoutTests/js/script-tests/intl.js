//@ skip if $hostOS == "windows"
description("This test checks the behavior of the Intl object as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

// 8 The Intl Object

// The Intl object is a single ordinary object.
shouldBeType("Intl", "Object");
shouldBe("typeof Intl", "'object'");
shouldBe("Object.prototype.toString.call(Intl)", "'[object Object]'");

// The value of the [[Prototype]] internal slot of the Intl object is the intrinsic object %ObjectPrototype%.
shouldBeTrue("Object.getPrototypeOf(Intl) === Object.prototype");

// The Intl object is not a function object.
// It does not have a [[Construct]] internal method; it is not possible to use the Intl object as a constructor with the new operator.
shouldThrow("new Intl", "'TypeError: Object is not a constructor (evaluating \\'new Intl\\')'");

// The Intl object does not have a [[Call]] internal method; it is not possible to invoke the Intl object as a function.
shouldThrow("Intl()", "'TypeError: Intl is not a function. (In \\'Intl()\\', \\'Intl\\' is an instance of Object)'");

// Has only the built-in Collator, DateTimeFormat, and NumberFormat, which are not enumerable.
shouldBe("Object.keys(Intl).length", "0");

// Is deletable, inferred from use of "Initial" in spec, consistent with other implementations.
var __Intl = Intl;
shouldBeTrue("delete Intl;");

function global() { return this; }
shouldBeFalse("'Intl' in global()");

Intl = __Intl;

// 8.2.1 Intl.getCanonicalLocales(locales)

// The value of the length property of the getCanonicalLocales method is 1.
shouldBe("Intl.getCanonicalLocales.length", "1");

// Returns Locales
shouldBeType("Intl.getCanonicalLocales()", "Array");
// Doesn't care about `this`.
shouldBe("Intl.getCanonicalLocales.call(null, 'en')", "[ 'en' ]");
shouldBe("Intl.getCanonicalLocales.call({}, 'en')", "[ 'en' ]");
shouldBe("Intl.getCanonicalLocales.call(1, 'en')", "[ 'en' ]");
// Ignores non-object, non-string list.
shouldBe("Intl.getCanonicalLocales(9)", "[]");
// Makes an array of tags.
shouldBe("Intl.getCanonicalLocales('en')", "[ 'en' ]");
// Handles array-like objects with holes.
shouldBe("Intl.getCanonicalLocales({ length: 4, 1: 'en', 0: 'es', 3: 'de' })", "[ 'es', 'en', 'de' ]");
// Deduplicates tags.
shouldBe("Intl.getCanonicalLocales([ 'en', 'pt', 'en', 'es' ])", "[ 'en', 'pt', 'es' ]");
// Canonicalizes tags.
shouldBe("Intl.getCanonicalLocales('En-laTn-us-variant2-variant1-1abc-U-ko-tRue-A-aa-aaa-x-RESERVED')", "[ 'en-Latn-US-variant2-variant1-1abc-a-aa-aaa-u-ko-true-x-reserved' ]");
// Replaces outdated tags.
shouldBe("Intl.getCanonicalLocales('no-bok')", "[ 'nb' ]");
// Canonicalizes private tags.
shouldBe("Intl.getCanonicalLocales('X-some-thing')", "[ 'x-some-thing' ]");
// Throws on problems with length, get, or toString.
shouldThrow("Intl.getCanonicalLocales(Object.create(null, { length: { get() { throw Error('a') } } }))", "'Error: a'");
shouldThrow("Intl.getCanonicalLocales(Object.create(null, { length: { value: 1 }, 0: { get() { throw Error('b') } } }))", "'Error: b'");
shouldThrow("Intl.getCanonicalLocales([ { toString() { throw Error('c') } } ])", "'Error: c'");
// Throws on bad tags.
shouldThrow("Intl.getCanonicalLocales([ 5 ])", "'TypeError: locale value must be a string or object'");
shouldThrow("Intl.getCanonicalLocales('')", "'RangeError: invalid language tag: '");
shouldThrow("Intl.getCanonicalLocales('a')", "'RangeError: invalid language tag: a'");
shouldThrow("Intl.getCanonicalLocales('abcdefghij')", "'RangeError: invalid language tag: abcdefghij'");
shouldThrow("Intl.getCanonicalLocales('#$')", "'RangeError: invalid language tag: #$'");
shouldThrow("Intl.getCanonicalLocales('en-@-abc')", "'RangeError: invalid language tag: en-@-abc'");
shouldThrow("Intl.getCanonicalLocales('en-u')", "'RangeError: invalid language tag: en-u'");
shouldThrow("Intl.getCanonicalLocales('en-u-kn-true-u-ko-true')", "'RangeError: invalid language tag: en-u-kn-true-u-ko-true'");
shouldThrow("Intl.getCanonicalLocales('en-x')", "'RangeError: invalid language tag: en-x'");
shouldThrow("Intl.getCanonicalLocales('en-*')", "'RangeError: invalid language tag: en-*'");
shouldThrow("Intl.getCanonicalLocales('en-')", "'RangeError: invalid language tag: en-'");
shouldThrow("Intl.getCanonicalLocales('en--US')", "'RangeError: invalid language tag: en--US'");
// Accepts valid tags
var validLanguageTags = [
    "de", // ISO 639 language code
    "de-DE", // + ISO 3166-1 country code
    "DE-de", // tags are case-insensitive
    "cmn", // ISO 639 language code
    "cmn-Hans", // + script code
    "CMN-hANS", // tags are case-insensitive
    "cmn-hans-cn", // + ISO 3166-1 country code
    "es-419", // + UN M.49 region code
    "es-419-u-nu-latn-cu-bob", // + Unicode locale extension sequence
    "i-klingon", // grandfathered tag
    "cmn-hans-cn-t-ca-u-ca-x-t-u", // singleton subtags can also be used as private use subtags
    "enochian-enochian", // language and variant subtags may be the same
    "de-gregory-u-ca-gregory", // variant and extension subtags may be the same
    "aa-a-foo-x-a-foo-bar", // variant subtags can also be used as private use subtags
    "x-en-US-12345", // anything goes in private use tags
    "x-12345-12345-en-US",
    "x-en-US-12345-12345",
    "x-en-u-foo",
    "x-en-u-foo-u-bar"
];
for (var validLanguageTag of validLanguageTags) {
    shouldNotThrow("Intl.getCanonicalLocales('" + validLanguageTag + "')");
}

