//@ skip if $hostOS == "windows" or $hostOS == "linux"
description("This test checks the behavior of Intl.PluralRules as described in the ECMAScript Internationalization API Specification.");

// https://tc39.github.io/ecma402/#pluralrules-objects
// 13.2 The Intl.PluralRules Constructor

// The PluralRules constructor is the %PluralRules% intrinsic object and a standard built-in property of the Intl object.
shouldBeType("Intl.PluralRules", "Function");

// 13.2.1 Intl.PluralRules ([ locales [, options ] ])

// If NewTarget is undefined, throw a TypeError exception.
shouldThrow("Intl.PluralRules()", "'TypeError: calling PluralRules constructor without new is invalid'");
shouldThrow("Intl.PluralRules.call({})", "'TypeError: calling PluralRules constructor without new is invalid'");

// Let pluralRules be ? OrdinaryCreateFromConstructor(newTarget, %PluralRulesPrototype%, « [[InitializedPluralRules]], [[Locale]], [[Type]], [[MinimumIntegerDigits]], [[MinimumFractionDigits]], [[MaximumFractionDigits]], [[MinimumSignificantDigits]], [[MaximumSignificantDigits]], [[PluralCategories]] »).
// Return ? InitializePluralRules(pluralRules, locales, options).
shouldThrow("new Intl.PluralRules('$')", "'RangeError: invalid language tag: $'");
shouldThrow("new Intl.PluralRules('en', null)", '"TypeError: null is not an object (evaluating \'new Intl.PluralRules(\'en\', null)\')"');
shouldBeType("new Intl.PluralRules()", "Intl.PluralRules");

// Subclassable
var classPrefix = "class DerivedPluralRules extends Intl.PluralRules {};";
shouldBeTrue(classPrefix + "(new DerivedPluralRules) instanceof DerivedPluralRules");
shouldBeTrue(classPrefix + "(new DerivedPluralRules) instanceof Intl.PluralRules");
shouldBeTrue(classPrefix + "new DerivedPluralRules().select(1) === 'one'");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(new DerivedPluralRules) === DerivedPluralRules.prototype");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(Object.getPrototypeOf(new DerivedPluralRules)) === Intl.PluralRules.prototype");

// 13.3 Properties of the Intl.PluralRules Constructor

// length property (whose value is 0)
shouldBe("Intl.PluralRules.length", "0");

// 13.3.1 Intl.PluralRules.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.PluralRules, 'prototype').writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.PluralRules, 'prototype').enumerable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.PluralRules, 'prototype').configurable");

// 13.3.2 Intl.PluralRules.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe("Intl.PluralRules.supportedLocalesOf.length", "1");

// Returns SupportedLocales
shouldBeType("Intl.PluralRules.supportedLocalesOf()", "Array");
// Doesn't care about `this`.
shouldBe("Intl.PluralRules.supportedLocalesOf.call(null, 'en')", "[ 'en' ]");
shouldBe("Intl.PluralRules.supportedLocalesOf.call({}, 'en')", "[ 'en' ]");
shouldBe("Intl.PluralRules.supportedLocalesOf.call(1, 'en')", "[ 'en' ]");
// Ignores non-object, non-string list.
shouldBe("Intl.PluralRules.supportedLocalesOf(9)", "[]");
// Makes an array of tags.
shouldBe("Intl.PluralRules.supportedLocalesOf('en')", "[ 'en' ]");
// Handles array-like objects with holes.
shouldBe("Intl.PluralRules.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })", "[ 'es', 'en', 'de' ]");
// Deduplicates tags.
shouldBe("Intl.PluralRules.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])", "[ 'en', 'pt', 'es' ]");
// Canonicalizes tags.
shouldBe("Intl.PluralRules.supportedLocalesOf('En-laTn-us-variant2-variant1-1abc-U-ko-tRue-A-aa-aaa-x-RESERVED')", "[ 'en-Latn-US-variant2-variant1-1abc-a-aa-aaa-u-ko-true-x-reserved' ]");
// Replaces outdated tags.
shouldBe("Intl.PluralRules.supportedLocalesOf('no-bok')", "[ 'nb' ]");
// Doesn't throw, but ignores private tags.
shouldBe("Intl.PluralRules.supportedLocalesOf('x-some-thing')", "[]");
// Throws on problems with length, get, or toString.
shouldThrow("Intl.PluralRules.supportedLocalesOf(Object.create(null, { length: { get() { throw Error('a') } } }))", "'Error: a'");
shouldThrow("Intl.PluralRules.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw Error('b') } } }))", "'Error: b'");
shouldThrow("Intl.PluralRules.supportedLocalesOf([ { toString() { throw Error('c') } } ])", "'Error: c'");
// Throws on bad tags.
shouldThrow("Intl.PluralRules.supportedLocalesOf([ 5 ])", "'TypeError: locale value must be a string or object'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('')", "'RangeError: invalid language tag: '");
shouldThrow("Intl.PluralRules.supportedLocalesOf('a')", "'RangeError: invalid language tag: a'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('abcdefghij')", "'RangeError: invalid language tag: abcdefghij'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('#$')", "'RangeError: invalid language tag: #$'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('en-@-abc')", "'RangeError: invalid language tag: en-@-abc'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('en-u')", "'RangeError: invalid language tag: en-u'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('en-u-kn-true-u-ko-true')", "'RangeError: invalid language tag: en-u-kn-true-u-ko-true'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('en-x')", "'RangeError: invalid language tag: en-x'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('en-*')", "'RangeError: invalid language tag: en-*'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('en-')", "'RangeError: invalid language tag: en-'");
shouldThrow("Intl.PluralRules.supportedLocalesOf('en--US')", "'RangeError: invalid language tag: en--US'");
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
    shouldNotThrow("Intl.PluralRules.supportedLocalesOf('" + validLanguageTag + "')");
}

// 13.4 Properties of the Intl.PluralRules Prototype Object

// The Intl.PluralRules prototype object is itself an ordinary object.
shouldBe("Object.getPrototypeOf(Intl.PluralRules.prototype)", "Object.prototype");

// 13.4.1 Intl.PluralRules.prototype.constructor
// The initial value of Intl.PluralRules.prototype.constructor is the intrinsic object %PluralRules%.
shouldBe("Intl.PluralRules.prototype.constructor", "Intl.PluralRules");

// 13.4.2 Intl.PluralRules.prototype [ @@toStringTag ]
// The initial value of the @@toStringTag property is the string value "Object".
shouldBe("Intl.PluralRules.prototype[Symbol.toStringTag]", "'Object'");
shouldBe("Object.prototype.toString.call(Intl.PluralRules.prototype)", "'[object Object]'");
// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, Symbol.toStringTag).writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, Symbol.toStringTag).enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, Symbol.toStringTag).configurable");

// 13.4.3 Intl.PluralRules.prototype.select (value)

// is a normal method.
shouldBeType("Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, 'select').value", "Function");
shouldBeTrue("Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, 'select').writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, 'select').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, 'select').configurable");
// the select function is simply inherited from the prototype.
shouldBeTrue("new Intl.PluralRules().select === new Intl.PluralRules().select");
// assume a length of 1 since it expects a value.
shouldBe("Intl.PluralRules.prototype.select.length", "1");

var defaultPluralRules = new Intl.PluralRules();

// Let pr be the this value.
// If Type(pr) is not Object, throw a TypeError exception.
shouldThrow("Intl.PluralRules.prototype.select.call(1)", "'TypeError: Intl.PluralRules.prototype.select called on value that\\'s not an object initialized as a PluralRules'");
// If pr does not have an [[InitializedPluralRules]] internal slot, throw a TypeError exception.
shouldThrow("Intl.PluralRules.prototype.select.call({})", "'TypeError: Intl.PluralRules.prototype.select called on value that\\'s not an object initialized as a PluralRules'");
// Let n be ? ToNumber(value).
shouldThrow("defaultPluralRules.select({ valueOf() { throw Error('4') } })", "'Error: 4'");
// Return ? ResolvePlural(pr, n).
shouldBe("defaultPluralRules.select(1)", "'one'");
shouldBe("Intl.PluralRules.prototype.select.call(defaultPluralRules, 1)", "'one'");
shouldBe("defaultPluralRules.select(0)", "'other'");
shouldBe("defaultPluralRules.select(-1)", "'one'");
shouldBe("defaultPluralRules.select(2)", "'other'");

// A few known examples
var englishOrdinals = new Intl.PluralRules('en', { type: 'ordinal' });
shouldBe("englishOrdinals.select(0)", "'other'");
shouldBe("englishOrdinals.select(1)", "'one'");
shouldBe("englishOrdinals.select(2)", "'two'");
shouldBe("englishOrdinals.select(3)", "'few'");
shouldBe("englishOrdinals.select(4)", "'other'");
shouldBe("englishOrdinals.select(11)", "'other'");
shouldBe("englishOrdinals.select(12)", "'other'");
shouldBe("englishOrdinals.select(13)", "'other'");
shouldBe("englishOrdinals.select(14)", "'other'");
shouldBe("englishOrdinals.select(21)", "'one'");
shouldBe("englishOrdinals.select(22)", "'two'");
shouldBe("englishOrdinals.select(23)", "'few'");
shouldBe("englishOrdinals.select(24)", "'other'");
shouldBe("englishOrdinals.select(101)", "'one'");
shouldBe("englishOrdinals.select(102)", "'two'");
shouldBe("englishOrdinals.select(103)", "'few'");
shouldBe("englishOrdinals.select(104)", "'other'");

var arabicCardinals = new Intl.PluralRules('ar');
shouldBe("arabicCardinals.select(0)", "'zero'");
shouldBe("arabicCardinals.select(1)", "'one'");
shouldBe("arabicCardinals.select(2)", "'two'");
shouldBe("arabicCardinals.select(3)", "'few'");
shouldBe("arabicCardinals.select(11)", "'many'");

// 13.4.4 Intl.PluralRules.prototype.resolvedOptions ()

shouldBe("Intl.PluralRules.prototype.resolvedOptions.length", "0");

// Let pr be the this value.
// If Type(pr) is not Object, throw a TypeError exception.
shouldThrow("Intl.PluralRules.prototype.resolvedOptions.call(5)", "'TypeError: Intl.PluralRules.prototype.resolvedOptions called on value that\\'s not an object initialized as a PluralRules'");
// If pr does not have an [[InitializedPluralRules]] internal slot, throw a TypeError exception.
shouldThrow("Intl.PluralRules.prototype.resolvedOptions.call({})", "'TypeError: Intl.PluralRules.prototype.resolvedOptions called on value that\\'s not an object initialized as a PluralRules'");
// Let options be ! ObjectCreate(%ObjectPrototype%).
shouldBeType("defaultPluralRules.resolvedOptions()", "Object");
shouldBeFalse("defaultPluralRules.resolvedOptions() === defaultPluralRules.resolvedOptions()");
// For each row of Table 8, except the header row, ...
// Return options.

// Defaults to en-US locale in test runner
shouldBe("defaultPluralRules.resolvedOptions().locale", "'en-US'");

// Defaults to cardinal.
shouldBe("defaultPluralRules.resolvedOptions().type", "'cardinal'");
shouldBe("defaultPluralRules.resolvedOptions().minimumIntegerDigits", "1");
shouldBe("defaultPluralRules.resolvedOptions().minimumFractionDigits", "0");
shouldBe("defaultPluralRules.resolvedOptions().maximumFractionDigits", "3");
shouldBe("defaultPluralRules.resolvedOptions().minimumSignificantDigits", "undefined");
shouldBe("defaultPluralRules.resolvedOptions().maximumSignificantDigits", "undefined");

// The option localeMatcher is processed correctly.
shouldThrow("new Intl.PluralRules('en', { localeMatcher: { toString() { throw 'nope' } } })", "'nope'");
shouldThrow("new Intl.PluralRules('en', { localeMatcher:'bad' })", '\'RangeError: localeMatcher must be either "lookup" or "best fit"\'');
shouldNotThrow("new Intl.PluralRules('en', { localeMatcher:'lookup' })");
shouldNotThrow("new Intl.PluralRules('en', { localeMatcher:'best fit' })");

// The option type is processed correctly.
shouldBe("new Intl.PluralRules('en', {type: 'cardinal'}).resolvedOptions().type", "'cardinal'");
shouldBe("new Intl.PluralRules('en', {type: 'ordinal'}).resolvedOptions().type", "'ordinal'");
shouldThrow("new Intl.PluralRules('en', {type: 'bad'})", '\'RangeError: type must be "cardinal" or "ordinal"\'');
shouldThrow("new Intl.PluralRules('en', {type: { toString() { throw 'badtype' } }})", "'badtype'");

// The option minimumIntegerDigits is processed correctly.
shouldBe("new Intl.PluralRules('en', {minimumIntegerDigits: 1}).resolvedOptions().minimumIntegerDigits", "1");
shouldBe("new Intl.PluralRules('en', {minimumIntegerDigits: '2'}).resolvedOptions().minimumIntegerDigits", "2");
shouldBe("new Intl.PluralRules('en', {minimumIntegerDigits: {valueOf() { return 3; }}}).resolvedOptions().minimumIntegerDigits", "3");
shouldBe("new Intl.PluralRules('en', {minimumIntegerDigits: 4.9}).resolvedOptions().minimumIntegerDigits", "4");
shouldBe("new Intl.PluralRules('en', {minimumIntegerDigits: 21}).resolvedOptions().minimumIntegerDigits", "21");
shouldThrow("new Intl.PluralRules('en', {minimumIntegerDigits: 0})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {minimumIntegerDigits: 22})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {minimumIntegerDigits: 0.9})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {minimumIntegerDigits: 21.1})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {minimumIntegerDigits: NaN})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {minimumIntegerDigits: Infinity})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', { get minimumIntegerDigits() { throw 42; } })", "'42'");
shouldThrow("new Intl.PluralRules('en', {minimumIntegerDigits: {valueOf() { throw 42; }}})", "'42'");

// The option minimumFractionDigits is processed correctly.
shouldBe("new Intl.PluralRules('en', {minimumFractionDigits: 0}).resolvedOptions().minimumFractionDigits", "0");
shouldBe("new Intl.PluralRules('en', {minimumFractionDigits: 0}).resolvedOptions().maximumFractionDigits", "3");
shouldBe("new Intl.PluralRules('en', {minimumFractionDigits: 6}).resolvedOptions().minimumFractionDigits", "6");
shouldBe("new Intl.PluralRules('en', {minimumFractionDigits: 6}).resolvedOptions().maximumFractionDigits", "6");
shouldThrow("new Intl.PluralRules('en', {minimumFractionDigits: -1})", "'RangeError: minimumFractionDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {minimumFractionDigits: 21})", "'RangeError: minimumFractionDigits is out of range'");

// The option maximumFractionDigits is processed correctly.
shouldBe("new Intl.PluralRules('en', {maximumFractionDigits: 6}).resolvedOptions().maximumFractionDigits", "6");
shouldThrow("new Intl.PluralRules('en', {minimumFractionDigits: 7, maximumFractionDigits: 6})", "'RangeError: maximumFractionDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {maximumFractionDigits: -1})", "'RangeError: maximumFractionDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {maximumFractionDigits: 21})", "'RangeError: maximumFractionDigits is out of range'");

// The option minimumSignificantDigits is processed correctly.
shouldBe("new Intl.PluralRules('en', {minimumSignificantDigits: 6}).resolvedOptions().minimumSignificantDigits", "6");
shouldBe("new Intl.PluralRules('en', {minimumSignificantDigits: 6}).resolvedOptions().maximumSignificantDigits", "21");
shouldThrow("new Intl.PluralRules('en', {minimumSignificantDigits: 0})", "'RangeError: minimumSignificantDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {minimumSignificantDigits: 22})", "'RangeError: minimumSignificantDigits is out of range'");

// The option maximumSignificantDigits is processed correctly.
shouldBe("new Intl.PluralRules('en', {maximumSignificantDigits: 6}).resolvedOptions().minimumSignificantDigits", "1");
shouldBe("new Intl.PluralRules('en', {maximumSignificantDigits: 6}).resolvedOptions().maximumSignificantDigits", "6");
shouldThrow("new Intl.PluralRules('en', {minimumSignificantDigits: 7, maximumSignificantDigits: 6})", "'RangeError: maximumSignificantDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {maximumSignificantDigits: 0})", "'RangeError: maximumSignificantDigits is out of range'");
shouldThrow("new Intl.PluralRules('en', {maximumSignificantDigits: 22})", "'RangeError: maximumSignificantDigits is out of range'");

// The number formatting impacts the final select.
shouldBe("new Intl.PluralRules('en', {maximumFractionDigits: 0}).select(1.4)", "'one'");
shouldBe("new Intl.PluralRules('en', {maximumSignificantDigits: 1}).select(1.4)", "'one'");
shouldBe("new Intl.PluralRules('en', {type: 'ordinal', maximumSignificantDigits: 2}).select(123)", "'other'");
shouldBe("new Intl.PluralRules('en', {type: 'ordinal', maximumSignificantDigits: 3}).select(123.4)", "'few'");

// These require ICU v59+
/*
shouldBe("new Intl.PluralRules('en', {minimumFractionDigits: 1}).select(1)", "'other'");
shouldBe("new Intl.PluralRules('en', {minimumSignificantDigits: 2}).select(1)", "'other'");

// Plural categories are correctly determined
shouldBeType("new Intl.PluralRules('en').resolvedOptions().pluralCategories", "Array");
shouldBe("new Intl.PluralRules('ar').resolvedOptions().pluralCategories.join()", "'zero,one,two,few,many,other'");
shouldBe("new Intl.PluralRules('en').resolvedOptions().pluralCategories.join()", "'one,other'");
shouldBe("new Intl.PluralRules('en', {type: 'ordinal'}).resolvedOptions().pluralCategories.join()", "'one,two,few,other'");
*/
