function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotThrow(func) {
    func();
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
}

// https://tc39.github.io/ecma402/#pluralrules-objects
// 13.2 The Intl.PluralRules Constructor

// The PluralRules constructor is the %PluralRules% intrinsic object and a standard built-in property of the Intl object.
shouldBe(Intl.PluralRules instanceof Function, true);

// 13.2.1 Intl.PluralRules ([ locales [, options ] ])

// If NewTarget is undefined, throw a TypeError exception.
shouldThrow(() => Intl.PluralRules(), TypeError);
shouldThrow(() => Intl.PluralRules.call({}), TypeError);

// Let pluralRules be ? OrdinaryCreateFromConstructor(newTarget, %PluralRulesPrototype%, « [[InitializedPluralRules]], [[Locale]], [[Type]], [[MinimumIntegerDigits]], [[MinimumFractionDigits]], [[MaximumFractionDigits]], [[MinimumSignificantDigits]], [[MaximumSignificantDigits]], [[PluralCategories]] »).
// Return ? InitializePluralRules(pluralRules, locales, options).
shouldThrow(() => new Intl.PluralRules('$'), RangeError);
shouldThrow(() => new Intl.PluralRules('en', null), TypeError);
shouldBe(new Intl.PluralRules() instanceof Intl.PluralRules, true);

// Subclassable
{
    class DerivedPluralRules extends Intl.PluralRules {};
    shouldBe((new DerivedPluralRules) instanceof DerivedPluralRules, true);
    shouldBe((new DerivedPluralRules) instanceof Intl.PluralRules, true);
    shouldBe(new DerivedPluralRules('en').select(1), 'one');
    shouldBe(Object.getPrototypeOf(new DerivedPluralRules), DerivedPluralRules.prototype);
    shouldBe(Object.getPrototypeOf(Object.getPrototypeOf(new DerivedPluralRules)), Intl.PluralRules.prototype);
}

// 13.3 Properties of the Intl.PluralRules Constructor

// length property (whose value is 0)
shouldBe(Intl.PluralRules.length, 0);

// 13.3.1 Intl.PluralRules.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules, 'prototype').configurable, false);

// 13.3.2 Intl.PluralRules.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe(Intl.PluralRules.supportedLocalesOf.length, 1);

// Returns SupportedLocales
shouldBe(Intl.PluralRules.supportedLocalesOf() instanceof Array, true);
// Doesn't care about `this`.
shouldBe(JSON.stringify(Intl.PluralRules.supportedLocalesOf.call(null, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.PluralRules.supportedLocalesOf.call({}, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.PluralRules.supportedLocalesOf.call(1, 'en')), '["en"]');
// Ignores non-object, non-string list.
shouldBe(JSON.stringify(Intl.PluralRules.supportedLocalesOf(9)), '[]');
// Makes an array of tags.
shouldBe(JSON.stringify(Intl.PluralRules.supportedLocalesOf('en')), '["en"]');
// Handles array-like objects with holes.
shouldBe(JSON.stringify(Intl.PluralRules.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })), '["es","en","de"]');
// Deduplicates tags.
shouldBe(JSON.stringify(Intl.PluralRules.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])), '["en","pt","es"]');
// Canonicalizes tags.
shouldBe(
    JSON.stringify(Intl.PluralRules.supportedLocalesOf('En-laTn-us-variAnt-fOObar-1abc-U-kn-tRue-A-aa-aaa-x-RESERVED')),
    $vm.icuVersion() >= 67
        ? '["en-Latn-US-1abc-foobar-variant-a-aa-aaa-u-kn-x-reserved"]'
        : '["en-Latn-US-variant-foobar-1abc-a-aa-aaa-u-kn-true-x-reserved"]'
);
// Replaces outdated tags.
shouldBe(JSON.stringify(Intl.PluralRules.supportedLocalesOf('no-bok')), '["nb"]');
// Doesn't throw, but ignores private tags.
shouldBe(JSON.stringify(Intl.PluralRules.supportedLocalesOf('x-some-thing')), '[]');
// Throws on problems with length, get, or toString.
shouldThrow(() => Intl.PluralRules.supportedLocalesOf(Object.create(null, { length: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf([ { toString() { throw new Error(); } } ]), Error);
// Throws on bad tags.
shouldThrow(() => Intl.PluralRules.supportedLocalesOf([ 5 ]), TypeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf(''), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('a'), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('abcdefghij'), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('#$'), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('en-@-abc'), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('en-u'), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('en-u-kn-true-u-ko-true'), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('en-x'), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('en-*'), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('en-'), RangeError);
shouldThrow(() => Intl.PluralRules.supportedLocalesOf('en--US'), RangeError);
// Accepts valid tags
var validLanguageTags = [
    'de', // ISO 639 language code
    'de-DE', // + ISO 3166-1 country code
    'DE-de', // tags are case-insensitive
    'cmn', // ISO 639 language code
    'cmn-Hans', // + script code
    'CMN-hANS', // tags are case-insensitive
    'cmn-hans-cn', // + ISO 3166-1 country code
    'es-419', // + UN M.49 region code
    'es-419-u-nu-latn-cu-bob', // + Unicode locale extension sequence
    'i-klingon', // grandfathered tag
    'cmn-hans-cn-t-ca-u-ca-x-t-u', // singleton subtags can also be used as private use subtags
    'enochian-enochian', // language and variant subtags may be the same
    'de-gregory-u-ca-gregory', // variant and extension subtags may be the same
    'aa-a-foo-x-a-foo-bar', // variant subtags can also be used as private use subtags
    'x-en-US-12345', // anything goes in private use tags
    'x-12345-12345-en-US',
    'x-en-US-12345-12345',
    'x-en-u-foo',
    'x-en-u-foo-u-bar'
];
for (var validLanguageTag of validLanguageTags)
    shouldNotThrow(() => Intl.PluralRules.supportedLocalesOf(validLanguageTag));

// 13.4 Properties of the Intl.PluralRules Prototype Object

// The Intl.PluralRules prototype object is itself an ordinary object.
shouldBe(Object.getPrototypeOf(Intl.PluralRules.prototype), Object.prototype);

// 13.4.1 Intl.PluralRules.prototype.constructor
// The initial value of Intl.PluralRules.prototype.constructor is the intrinsic object %PluralRules%.
shouldBe(Intl.PluralRules.prototype.constructor, Intl.PluralRules);

// 13.4.2 Intl.PluralRules.prototype [ @@toStringTag ]
// The initial value of the @@toStringTag property is the string value "Intl.PluralRules".
shouldBe(Intl.PluralRules.prototype[Symbol.toStringTag], 'Intl.PluralRules');
shouldBe(Object.prototype.toString.call(Intl.PluralRules.prototype), '[object Intl.PluralRules]');
// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, Symbol.toStringTag).writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, Symbol.toStringTag).enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, Symbol.toStringTag).configurable, true);

// 13.4.3 Intl.PluralRules.prototype.select (value)

// is a normal method.
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, 'select').value instanceof Function, true);
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, 'select').writable, true);
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, 'select').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.PluralRules.prototype, 'select').configurable, true);
// the select function is simply inherited from the prototype.
shouldBe(new Intl.PluralRules().select, new Intl.PluralRules().select);
// assume a length of 1 since it expects a value.
shouldBe(Intl.PluralRules.prototype.select.length, 1);

var defaultPluralRules = new Intl.PluralRules('en');

// Let pr be the this value.
// If Type(pr) is not Object, throw a TypeError exception.
shouldThrow(() => Intl.PluralRules.prototype.select.call(1), TypeError);
// If pr does not have an [[InitializedPluralRules]] internal slot, throw a TypeError exception.
shouldThrow(() => Intl.PluralRules.prototype.select.call({}), TypeError);
// Let n be ? ToNumber(value).
shouldThrow(() => defaultPluralRules.select({ valueOf() { throw new Error(); } }), Error);
// Return ? ResolvePlural(pr, n).
shouldBe(defaultPluralRules.select(1), 'one');
shouldBe(Intl.PluralRules.prototype.select.call(defaultPluralRules, 1), 'one');
shouldBe(defaultPluralRules.select(0), 'other');
shouldBe(defaultPluralRules.select(-1), 'one');
shouldBe(defaultPluralRules.select(2), 'other');

// A few known examples
var englishOrdinals = new Intl.PluralRules('en', { type: 'ordinal' });
shouldBe(englishOrdinals.select(0), 'other');
shouldBe(englishOrdinals.select(1), 'one');
shouldBe(englishOrdinals.select(2), 'two');
shouldBe(englishOrdinals.select(3), 'few');
shouldBe(englishOrdinals.select(4), 'other');
shouldBe(englishOrdinals.select(11), 'other');
shouldBe(englishOrdinals.select(12), 'other');
shouldBe(englishOrdinals.select(13), 'other');
shouldBe(englishOrdinals.select(14), 'other');
shouldBe(englishOrdinals.select(21), 'one');
shouldBe(englishOrdinals.select(22), 'two');
shouldBe(englishOrdinals.select(23), 'few');
shouldBe(englishOrdinals.select(24), 'other');
shouldBe(englishOrdinals.select(101), 'one');
shouldBe(englishOrdinals.select(102), 'two');
shouldBe(englishOrdinals.select(103), 'few');
shouldBe(englishOrdinals.select(104), 'other');

var arabicCardinals = new Intl.PluralRules('ar');
shouldBe(arabicCardinals.select(0), 'zero');
shouldBe(arabicCardinals.select(1), 'one');
shouldBe(arabicCardinals.select(2), 'two');
shouldBe(arabicCardinals.select(3), 'few');
shouldBe(arabicCardinals.select(11), 'many');

// 13.4.4 Intl.PluralRules.prototype.resolvedOptions ()

shouldBe(Intl.PluralRules.prototype.resolvedOptions.length, 0);

// Let pr be the this value.
// If Type(pr) is not Object, throw a TypeError exception.
shouldThrow(() => Intl.PluralRules.prototype.resolvedOptions.call(5), TypeError);
// If pr does not have an [[InitializedPluralRules]] internal slot, throw a TypeError exception.
shouldThrow(() => Intl.PluralRules.prototype.resolvedOptions.call({}), TypeError);
// Let options be ! ObjectCreate(%ObjectPrototype%).
shouldBe(defaultPluralRules.resolvedOptions() instanceof Object, true);
shouldBe(defaultPluralRules.resolvedOptions() !== defaultPluralRules.resolvedOptions(), true);
// For each row of Table 8, except the header row, ...
// Return options.

// Defaults to cardinal.
shouldBe(defaultPluralRules.resolvedOptions().type, 'cardinal');
shouldBe(defaultPluralRules.resolvedOptions().minimumIntegerDigits, 1);
shouldBe(defaultPluralRules.resolvedOptions().minimumFractionDigits, 0);
shouldBe(defaultPluralRules.resolvedOptions().maximumFractionDigits, 3);
shouldBe(defaultPluralRules.resolvedOptions().minimumSignificantDigits, undefined);
shouldBe(defaultPluralRules.resolvedOptions().maximumSignificantDigits, undefined);

// The option localeMatcher is processed correctly.
shouldThrow(() => new Intl.PluralRules('en', { localeMatcher: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.PluralRules('en', { localeMatcher:'bad' }), RangeError);
shouldNotThrow(() => new Intl.PluralRules('en', { localeMatcher:'lookup' }));
shouldNotThrow(() => new Intl.PluralRules('en', { localeMatcher:'best fit' }));

// The option type is processed correctly.
shouldBe(new Intl.PluralRules('en', {type: 'cardinal'}).resolvedOptions().type, 'cardinal');
shouldBe(new Intl.PluralRules('en', {type: 'ordinal'}).resolvedOptions().type, 'ordinal');
shouldThrow(() => new Intl.PluralRules('en', {type: 'bad'}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {type: { toString() { throw new Error(); } }}), Error);

// The option minimumIntegerDigits is processed correctly.
shouldBe(new Intl.PluralRules('en', {minimumIntegerDigits: 1}).resolvedOptions().minimumIntegerDigits, 1);
shouldBe(new Intl.PluralRules('en', {minimumIntegerDigits: '2'}).resolvedOptions().minimumIntegerDigits, 2);
shouldBe(new Intl.PluralRules('en', {minimumIntegerDigits: {valueOf() { return 3; }}}).resolvedOptions().minimumIntegerDigits, 3);
shouldBe(new Intl.PluralRules('en', {minimumIntegerDigits: 4.9}).resolvedOptions().minimumIntegerDigits, 4);
shouldBe(new Intl.PluralRules('en', {minimumIntegerDigits: 21}).resolvedOptions().minimumIntegerDigits, 21);
shouldThrow(() => new Intl.PluralRules('en', {minimumIntegerDigits: 0}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {minimumIntegerDigits: 22}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {minimumIntegerDigits: 0.9}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {minimumIntegerDigits: 21.1}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {minimumIntegerDigits: NaN}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {minimumIntegerDigits: Infinity}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', { get minimumIntegerDigits() { throw new Error(); } }), Error);
shouldThrow(() => new Intl.PluralRules('en', {minimumIntegerDigits: {valueOf() { throw new Error(); }}}), Error);

// The option minimumFractionDigits is processed correctly.
shouldBe(new Intl.PluralRules('en', {minimumFractionDigits: 0}).resolvedOptions().minimumFractionDigits, 0);
shouldBe(new Intl.PluralRules('en', {minimumFractionDigits: 0}).resolvedOptions().maximumFractionDigits, 3);
shouldBe(new Intl.PluralRules('en', {minimumFractionDigits: 6}).resolvedOptions().minimumFractionDigits, 6);
shouldBe(new Intl.PluralRules('en', {minimumFractionDigits: 6}).resolvedOptions().maximumFractionDigits, 6);
shouldThrow(() => new Intl.PluralRules('en', {minimumFractionDigits: -1}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {minimumFractionDigits: 21}), RangeError);

// The option maximumFractionDigits is processed correctly.
shouldBe(new Intl.PluralRules('en', {maximumFractionDigits: 6}).resolvedOptions().maximumFractionDigits, 6);
shouldThrow(() => new Intl.PluralRules('en', {minimumFractionDigits: 7, maximumFractionDigits: 6}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {maximumFractionDigits: -1}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {maximumFractionDigits: 21}), RangeError);

// The option minimumSignificantDigits is processed correctly.
shouldBe(new Intl.PluralRules('en', {minimumSignificantDigits: 6}).resolvedOptions().minimumSignificantDigits, 6);
shouldBe(new Intl.PluralRules('en', {minimumSignificantDigits: 6}).resolvedOptions().maximumSignificantDigits, 21);
shouldThrow(() => new Intl.PluralRules('en', {minimumSignificantDigits: 0}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {minimumSignificantDigits: 22}), RangeError);

// The option maximumSignificantDigits is processed correctly.
shouldBe(new Intl.PluralRules('en', {maximumSignificantDigits: 6}).resolvedOptions().minimumSignificantDigits, 1);
shouldBe(new Intl.PluralRules('en', {maximumSignificantDigits: 6}).resolvedOptions().maximumSignificantDigits, 6);
shouldThrow(() => new Intl.PluralRules('en', {minimumSignificantDigits: 7, maximumSignificantDigits: 6}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {maximumSignificantDigits: 0}), RangeError);
shouldThrow(() => new Intl.PluralRules('en', {maximumSignificantDigits: 22}), RangeError);

// The number formatting impacts the final select.
shouldBe(new Intl.PluralRules('en', {maximumFractionDigits: 0}).select(1.4), 'one');
shouldBe(new Intl.PluralRules('en', {maximumSignificantDigits: 1}).select(1.4), 'one');
shouldBe(new Intl.PluralRules('en', {type: 'ordinal', maximumSignificantDigits: 2}).select(123), 'other');
shouldBe(new Intl.PluralRules('en', {type: 'ordinal', maximumSignificantDigits: 3}).select(123.4), 'few');

shouldBe(new Intl.PluralRules('en', {minimumFractionDigits: 1}).select(1), 'other');
shouldBe(new Intl.PluralRules('en', {minimumSignificantDigits: 2}).select(1), 'other');

// Plural categories are correctly determined
shouldBe(new Intl.PluralRules('en').resolvedOptions().pluralCategories instanceof Array, true);
shouldBe(new Intl.PluralRules('ar').resolvedOptions().pluralCategories.join(), 'few,many,one,two,zero,other');
shouldBe(new Intl.PluralRules('en').resolvedOptions().pluralCategories.join(), 'one,other');
shouldBe(new Intl.PluralRules('en', {type: 'ordinal'}).resolvedOptions().pluralCategories.join(), 'few,one,two,other');
