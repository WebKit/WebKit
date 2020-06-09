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

function explicitTrueBeforeICU67() {
    return $vm.icuVersion() < 67 ? '-true' : '';
}

// 10.1 The Intl.Collator Constructor

// The Intl.Collator constructor is a standard built-in property of the Intl object.
shouldBe(Intl.Collator instanceof Function, true);

// 10.1.2 Intl.Collator([ locales [, options]])
shouldBe(Intl.Collator() instanceof Intl.Collator, true);
shouldBe(Intl.Collator.call({}) instanceof Intl.Collator, true);
shouldBe(new Intl.Collator() instanceof Intl.Collator, true);
shouldBe(Intl.Collator('en') instanceof Intl.Collator, true);
shouldBe(Intl.Collator(42) instanceof Intl.Collator, true);
shouldThrow(() => Intl.Collator(null), TypeError);
shouldThrow(() => Intl.Collator({ get length() { throw new Error(); } }), Error);
shouldBe(Intl.Collator('en', { }) instanceof Intl.Collator, true);
shouldBe(Intl.Collator('en', 42) instanceof Intl.Collator, true);
shouldThrow(() => Intl.Collator('en', null), TypeError);

// Subclassable
{
    class DerivedCollator extends Intl.Collator {}
    shouldBe((new DerivedCollator) instanceof DerivedCollator, true);
    shouldBe((new DerivedCollator) instanceof Intl.Collator, true);
    shouldBe(new DerivedCollator('en').compare('a', 'b'), -1);
    shouldBe(Object.getPrototypeOf(new DerivedCollator), DerivedCollator.prototype);
    shouldBe(Object.getPrototypeOf(Object.getPrototypeOf(new DerivedCollator)), Intl.Collator.prototype);
}

function testCollator(collator, possibleOptionDifferences) {
    var possibleOptions = possibleOptionDifferences.map((difference) => {
        var defaultOptions = {
            locale: undefined,
            usage: "sort",
            sensitivity: "variant",
            ignorePunctuation: false,
            collation: "default",
            numeric: false,
            caseFirst: "false"
        };
        Object.assign(defaultOptions, difference);
        return JSON.stringify(defaultOptions);
    });
    var actualOptions = JSON.stringify(collator.resolvedOptions());
    return possibleOptions.includes(actualOptions);
}

// Locale is processed correctly.
shouldBe(testCollator(Intl.Collator('en'), [{locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('eN-uS'), [{locale: 'en-US'}]), true);
shouldBe(testCollator(Intl.Collator(['en', 'de']), [{locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('de'), [{locale: 'de'}]), true);

// The 'co' key is processed correctly.
shouldBe(testCollator(Intl.Collator('en-u-co-eor'), [{locale: 'en-u-co-eor', collation: 'eor'}, {locale: 'en'}]), true);
shouldBe(testCollator(new Intl.Collator('en-u-co-eor'), [{locale: 'en-u-co-eor', collation: 'eor'}, {locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('En-U-Co-Eor'), [{locale: 'en-u-co-eor', collation: 'eor'}, {locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('en-u-co-phonebk'), [{locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('en-u-co-standard'), [{locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('en-u-co-search'), [{locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('en-u-co-abcd'), [{locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('de-u-co-phonebk'), [{locale: 'de-u-co-phonebk', collation: 'phonebk'}, {locale: 'de'}]), true);

// The 'kn' key is processed correctly.
shouldBe(testCollator(Intl.Collator('en-u-kn'), [{locale: 'en-u-kn' + explicitTrueBeforeICU67(), numeric: true}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kn-true'), [{locale: 'en-u-kn' + explicitTrueBeforeICU67(), numeric: true}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kn-false'), [{locale: 'en-u-kn-false', numeric: false}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kn-abcd'), [{locale: 'en'}]), true);

// The 'kf' key is processed correctly.
shouldBe(testCollator(Intl.Collator('en-u-kf'), [{locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kf-upper'), [{locale: 'en-u-kf-upper', caseFirst: 'upper'}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kf-lower'), [{locale: 'en-u-kf-lower', caseFirst: 'lower'}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kf-false'), [{locale: 'en-u-kf-false', caseFirst: 'false'}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kf-true'), [{locale: 'en'}]), true);

// Ignores irrelevant extension keys.
shouldBe(testCollator(Intl.Collator('en-u-aa-aaaa-kf-upper-bb-bbbb-co-eor-cc-cccc-y-yyd'), [{locale: 'en-u-co-eor-kf-upper', collation: 'eor', caseFirst: 'upper'}, {locale: 'en-u-kf-upper', caseFirst: 'upper'}]), true);

// Ignores other extensions.
shouldBe(testCollator(Intl.Collator('en-u-kf-upper-a-aa'), [{locale: 'en-u-kf-upper', caseFirst: 'upper'}]), true);
shouldBe(testCollator(Intl.Collator('en-a-aa-u-kf-upper'), [{locale: 'en-u-kf-upper', caseFirst: 'upper'}]), true);
shouldBe(testCollator(Intl.Collator('en-a-aa-u-kf-upper-b-bb'), [{locale: 'en-u-kf-upper', caseFirst: 'upper'}]), true);

// The option usage is processed correctly.
shouldBe(testCollator(Intl.Collator('en', {usage: 'sort'}), [{locale: 'en', usage: 'sort'}]), true);
shouldBe(testCollator(Intl.Collator('en', {usage: 'search'}), [{locale: 'en', usage: 'search'}]), true);
shouldThrow(() => Intl.Collator('en', {usage: 'Sort'}), RangeError);
shouldThrow(() => Intl.Collator('en', { get usage() { throw new Error(); } }), Error);
shouldThrow(() => Intl.Collator('en', {usage: {toString() { throw new Error(); }}}), Error);

// The option localeMatcher is processed correctly.
shouldBe(testCollator(Intl.Collator('en', {localeMatcher: 'lookup'}), [{locale: 'en'}]), true);
shouldBe(testCollator(Intl.Collator('en', {localeMatcher: 'best fit'}), [{locale: 'en'}]), true);
shouldThrow(() => Intl.Collator('en', {localeMatcher: 'LookUp'}), RangeError);
shouldThrow(() => Intl.Collator('en', { get localeMatcher() { throw new Error(); } }), Error);

// The option numeric is processed correctly.
shouldBe(testCollator(Intl.Collator('en', {numeric: true}), [{locale: 'en', numeric: true}]), true);
shouldBe(testCollator(Intl.Collator('en', {numeric: false}), [{locale: 'en', numeric: false}]), true);
shouldBe(testCollator(Intl.Collator('en', {numeric: 'false'}), [{locale: 'en', numeric: true}]), true);
shouldBe(testCollator(Intl.Collator('en', {numeric: { }}), [{locale: 'en', numeric: true}]), true);
shouldThrow(() => Intl.Collator('en', { get numeric() { throw new Error(); } }), Error);

// The option caseFirst is processed correctly.
shouldBe(testCollator(Intl.Collator('en', {caseFirst: 'upper'}), [{locale: 'en', caseFirst: 'upper'}]), true);
shouldBe(testCollator(Intl.Collator('en', {caseFirst: 'lower'}), [{locale: 'en', caseFirst: 'lower'}]), true);
shouldBe(testCollator(Intl.Collator('en', {caseFirst: 'false'}), [{locale: 'en', caseFirst: 'false'}]), true);
shouldBe(testCollator(Intl.Collator('en', {caseFirst: false}), [{locale: 'en', caseFirst: 'false'}]), true);
shouldThrow(() => Intl.Collator('en', {caseFirst: 'true'}), RangeError);
shouldThrow(() => Intl.Collator('en', { get caseFirst() { throw new Error(); } }), Error);

// The option sensitivity is processed correctly.
shouldBe(testCollator(Intl.Collator('en', {sensitivity: 'base'}), [{locale: 'en', sensitivity: 'base'}]), true);
shouldBe(testCollator(Intl.Collator('en', {sensitivity: 'accent'}), [{locale: 'en', sensitivity: 'accent'}]), true);
shouldBe(testCollator(Intl.Collator('en', {sensitivity: 'case'}), [{locale: 'en', sensitivity: 'case'}]), true);
shouldBe(testCollator(Intl.Collator('en', {sensitivity: 'variant'}), [{locale: 'en', sensitivity: 'variant'}]), true);
shouldThrow(() => Intl.Collator('en', {sensitivity: 'abcd'}), RangeError);
shouldThrow(() => Intl.Collator('en', { get sensitivity() { throw new Error(); } }), Error);

// The option ignorePunctuation is processed correctly.
shouldBe(testCollator(Intl.Collator('en', {ignorePunctuation: true}), [{locale: 'en', ignorePunctuation: true}]), true);
shouldBe(testCollator(Intl.Collator('en', {ignorePunctuation: false}), [{locale: 'en', ignorePunctuation: false}]), true);
shouldBe(testCollator(Intl.Collator('en', {ignorePunctuation: 'false'}), [{locale: 'en', ignorePunctuation: true}]), true);
shouldThrow(() => Intl.Collator('en', { get ignorePunctuation() { throw new Error(); } }), Error);

// Options override the language tag.
shouldBe(testCollator(Intl.Collator('en-u-kn-true', {numeric: false}), [{locale: 'en', numeric: false}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kn-false', {numeric: true}), [{locale: 'en', numeric: true}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kn-true', {numeric: true}), [{locale: 'en-u-kn' + explicitTrueBeforeICU67(), numeric: true}]), true);
shouldBe(testCollator(Intl.Collator('en-u-kn-false', {numeric: false}), [{locale: 'en-u-kn-false', numeric: false}]), true);

// Options and extension keys are processed correctly.
shouldBe(testCollator(Intl.Collator('en-a-aa-u-kn-false-co-eor-kf-upper-b-bb', {usage: 'sort', numeric: true, caseFirst: 'lower', sensitivity: 'case', ignorePunctuation: true}), [{locale: 'en-u-co-eor', usage: 'sort', sensitivity: 'case', ignorePunctuation: true, collation: 'eor', numeric: true, caseFirst: 'lower'}, {locale: 'en', usage: 'sort', sensitivity: 'case', ignorePunctuation: true, numeric: true, caseFirst: 'lower'}]), true);

// 10.2 Properties of the Intl.Collator Constructor

// length property (whose value is 0)
shouldBe(Intl.Collator.length, 0);

// 10.2.1 Intl.Collator.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator, 'prototype').configurable, false);

// 10.2.2 Intl.Collator.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe(Intl.Collator.supportedLocalesOf.length, 1);

// Returns SupportedLocales.
shouldBe(Intl.Collator.supportedLocalesOf() instanceof Array, true);
// Doesn't care about `this`.
shouldBe(JSON.stringify(Intl.Collator.supportedLocalesOf.call(null, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.Collator.supportedLocalesOf.call({}, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.Collator.supportedLocalesOf.call(1, 'en')), '["en"]');
// Ignores non-object, non-string list.
shouldBe(JSON.stringify(Intl.Collator.supportedLocalesOf(9)), '[]');
// Makes an array of tags.
shouldBe(JSON.stringify(Intl.Collator.supportedLocalesOf('en')), '["en"]');
// Handles array-like objects with holes.
shouldBe(JSON.stringify(Intl.Collator.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })), '["es","en","de"]');
// Deduplicates tags.
shouldBe(JSON.stringify(Intl.Collator.supportedLocalesOf(['en', 'pt', 'en', 'es'])), '["en","pt","es"]');
// Canonicalizes tags.
shouldBe(
    JSON.stringify(Intl.Collator.supportedLocalesOf('En-laTn-us-variAnt-fOObar-1abc-U-kn-tRue-A-aa-aaa-x-RESERVED')),
    $vm.icuVersion() >= 67
        ? '["en-Latn-US-1abc-foobar-variant-a-aa-aaa-u-kn-x-reserved"]'
        : '["en-Latn-US-variant-foobar-1abc-a-aa-aaa-u-kn-true-x-reserved"]'
);
// Replaces outdated tags.
shouldBe(JSON.stringify(Intl.Collator.supportedLocalesOf('no-bok')), '["nb"]');
// Doesn't throw, but ignores private tags.
shouldBe(JSON.stringify(Intl.Collator.supportedLocalesOf('x-some-thing')), '[]');
// Throws on problems with length, get, or toString.
shouldThrow(() => Intl.Collator.supportedLocalesOf(Object.create(null, { length: { get() { throw Error() } } })), Error);
shouldThrow(() => Intl.Collator.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw Error() } } })), Error);
shouldThrow(() => Intl.Collator.supportedLocalesOf([ { toString() { throw Error() } } ]), Error);
// Throws on bad tags.
shouldThrow(() => Intl.Collator.supportedLocalesOf([ 5 ]), TypeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf(''), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('a'), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('abcdefghij'), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('#$'), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('en-@-abc'), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('en-u'), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('en-u-kn-true-u-ko-true'), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('en-x'), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('en-*'), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('en-'), RangeError);
shouldThrow(() => Intl.Collator.supportedLocalesOf('en--US'), RangeError);
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
    shouldNotThrow(() => Intl.Collator.supportedLocalesOf(validLanguageTag));

// 10.3 Properties of the Intl.Collator Prototype Object

// The Intl.Collator prototype object is itself an ordinary object.
shouldBe(Object.getPrototypeOf(Intl.Collator.prototype), Object.prototype);

// 10.3.1 Intl.Collator.prototype.constructor
// The initial value of Intl.Collator.prototype.constructor is the intrinsic object %Collator%.
shouldBe(Intl.Collator.prototype.constructor, Intl.Collator);

// 10.3.2 Intl.Collator.prototype [ @@toStringTag ]
// The initial value of the @@toStringTag property is the string value "Intl.Collator".
shouldBe(Intl.Collator.prototype[Symbol.toStringTag], 'Intl.Collator');
shouldBe(Object.prototype.toString.call(Intl.Collator.prototype), '[object Intl.Collator]');
// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator.prototype, Symbol.toStringTag).writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator.prototype, Symbol.toStringTag).enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator.prototype, Symbol.toStringTag).configurable, true);

// 10.3.3 Intl.Collator.prototype.compare

// This named accessor property returns a function that compares two strings according to the sort order of this Collator object.
var defaultCollator = Intl.Collator('en');
shouldBe(defaultCollator.compare instanceof Function, true);

// The value of the [[Get]] attribute is a function
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').get instanceof Function, true);

// The value of the [[Set]] attribute is undefined.
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').set, undefined);

// Match Firefox where unspecifed.
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').configurable, true);

// The value of F’s length property is 2.
shouldBe(Intl.Collator().compare.length, 2);

// Throws on non-Collator this.
shouldThrow(() => Intl.Collator.prototype.compare, TypeError);
shouldThrow(() => Object.defineProperty({}, 'compare', Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare')).compare, TypeError);

// The compare function is unique per instance.
shouldBe(new Intl.Collator().compare !== new Intl.Collator().compare, true);

// 10.3.4 Collator Compare Functions

// 1. Let collator be the this value.
// 2. Assert: Type(collator) is Object and collator has an [[initializedCollator]] internal slot whose value is true.
// This should not be reachable, since compare is bound to an initialized collator.

// 3. If x is not provided, let x be undefined.
// 4. If y is not provided, let y be undefined.
// 5. Let X be ToString(x).
// 6. ReturnIfAbrupt(X).
var badCalls = 0;
shouldThrow(() => defaultCollator.compare({ toString() { throw new Error(); } }, { toString() { ++badCalls; return ''; } }), Error);
shouldBe(badCalls, 0);

// 7. Let Y be ToString(y).
// 8. ReturnIfAbrupt(Y).
shouldThrow(() => defaultCollator.compare('a', { toString() { throw new Error(); } }), Error);

// Compare is bound, so calling with alternate "this" has no effect.
shouldBe(defaultCollator.compare.call(null, 'a', 'b'), -1);
shouldBe(defaultCollator.compare.call(Intl.Collator('en', { sensitivity:'base' }), 'A', 'a'), 1);
shouldBe(defaultCollator.compare.call(5, 'a', 'b'), -1);

// Test comparing undefineds.
shouldBe(defaultCollator.compare(), 0);
shouldBe(defaultCollator.compare('undefinec'), -1);
shouldBe(defaultCollator.compare('undefinee'), 1);

// Test locales.
shouldBe(Intl.Collator('en').compare('ä', 'z'), -1);
shouldBe(Intl.Collator('sv').compare('ä', 'z'), 1);

// Test collations.
shouldBe(Intl.Collator('de').compare('ö', 'od'), -1);
shouldBe(Intl.Collator('de-u-co-phonebk').resolvedOptions().collation != 'phonebk' || Intl.Collator('de-u-co-phonebk').compare('ö', 'od') == 1, true);

// Test the usage option.
var sortCompare = Intl.Collator('en', {usage: 'sort'}).compare;
shouldBe(JSON.stringify(['A', 'B', 'C', 'a', 'b', 'c'].sort(sortCompare)), '["a","A","b","B","c","C"]');

var searchCompare = Intl.Collator('en', {usage: 'search', sensitivity: 'base'}).compare;
shouldBe(JSON.stringify(['A', 'B', 'C', 'a', 'b', 'c'].filter(x => (searchCompare(x, 'b') == 0))), '["B","b"]');

// Test the sensitivity option.
var baseCompare = Intl.Collator('en', {sensitivity: 'base'}).compare;
shouldBe(baseCompare('a', 'b'), -1);
shouldBe(baseCompare('a', 'ä'), 0);
shouldBe(baseCompare('a', 'A'), 0);
shouldBe(baseCompare('a', 'ⓐ'), 0);

var accentCompare = Intl.Collator('en', {sensitivity: 'accent'}).compare;
shouldBe(accentCompare('a', 'b'), -1);
shouldBe(accentCompare('a', 'ä'), -1);
shouldBe(accentCompare('a', 'A'), 0);
shouldBe(accentCompare('a', 'ⓐ'), 0);

var caseCompare = Intl.Collator('en', {sensitivity: 'case'}).compare;
shouldBe(caseCompare('a', 'b'), -1);
shouldBe(caseCompare('a', 'ä'), 0);
shouldBe(caseCompare('a', 'A'), -1);
shouldBe(caseCompare('a', 'ⓐ'), 0);

var variantCompare = Intl.Collator('en', {sensitivity: 'variant'}).compare;
shouldBe(variantCompare('a', 'b'), -1);
shouldBe(variantCompare('a', 'ä'), -1);
shouldBe(variantCompare('a', 'A'), -1);
shouldBe(variantCompare('a', 'ⓐ'), -1);

// Test the numeric option.
var nonNumericCompare = Intl.Collator('en', {numeric: false}).compare;
shouldBe(nonNumericCompare('1', '2'), -1);
shouldBe(nonNumericCompare('2', '10'), 1);
shouldBe(nonNumericCompare('01', '1'), -1);
shouldBe(nonNumericCompare('๑', '๒'), -1);
shouldBe(nonNumericCompare('๒', '๑๐'), 1);
shouldBe(nonNumericCompare('๐๑', '๑'), -1);

var numericCompare = Intl.Collator('en', {numeric: true}).compare;
shouldBe(numericCompare('1', '2'), -1);
shouldBe(numericCompare('2', '10'), -1);
shouldBe(numericCompare('01', '1'), 0);
shouldBe(numericCompare('๑', '๒'), -1);
shouldBe(numericCompare('๒', '๑๐'), -1);
shouldBe(numericCompare('๐๑', '๑'), 0);

// Test the caseFirst option.
shouldBe(Intl.Collator('en', {caseFirst: 'upper'}).compare('a', 'A'), 1);
shouldBe(Intl.Collator('en', {caseFirst: 'lower'}).compare('a', 'A'), -1);
shouldBe(Intl.Collator('en', {caseFirst: 'false'}).compare('a', 'A'), -1);

// Test the ignorePunctuation option.
var notIgnorePunctuationCompare = Intl.Collator('en', {ignorePunctuation: false}).compare;
var ignorePunctuationCompare = Intl.Collator('en', {ignorePunctuation: true}).compare;
var punctuations = ['\'', '"', ':', ';', ',', '-', '!', '.', '?'];
for (let punctuation of punctuations) {
    shouldBe(notIgnorePunctuationCompare('ab', `a${punctuation}a`), 1);
    shouldBe(notIgnorePunctuationCompare('ab', `a${punctuation}b`), 1);
    shouldBe(notIgnorePunctuationCompare('ab', `a${punctuation}c`), 1);

    shouldBe(ignorePunctuationCompare('ab', `a${punctuation}a`), 1);
    shouldBe(ignorePunctuationCompare('ab', `a${punctuation}b`), 0);
    shouldBe(ignorePunctuationCompare('ab', `a${punctuation}c`), -1);
}

// FIXME: The result of ignorePunctuationCompare('ab', 'a b') should be 1. The whitespace shouldn't be ignored.
shouldBe(ignorePunctuationCompare('ab', 'a b'), 0);

// Returns 0 for strings that are canonically equivalent.
shouldBe(Intl.Collator('en').compare('A\u0308', '\u00c4'), 0); // A + umlaut == A-umlaut.
shouldBe(Intl.Collator('en').compare('A\u0327\u030a', '\u212b\u0327'), 0); // A + cedilla + ring == A-ring + cedilla.

// 10.3.5 Intl.Collator.prototype.resolvedOptions ()

shouldBe(Intl.Collator.prototype.resolvedOptions.length, 0);

// Returns a new object whose properties and attributes are set as if constructed by an object literal.
shouldBe(defaultCollator.resolvedOptions() instanceof Object, true);

// Returns a new object each time.
shouldBe(defaultCollator.resolvedOptions() !== defaultCollator.resolvedOptions(), true);

// Throws on non-Collator this.
shouldThrow(() => Intl.Collator.prototype.resolvedOptions.call(5), TypeError);

// Returns the default options.
{
    let options = defaultCollator.resolvedOptions();
    delete options.locale;
    shouldBe(JSON.stringify(options), '{"usage":"sort","sensitivity":"variant","ignorePunctuation":false,"collation":"default","numeric":false,"caseFirst":"false"}');
}

shouldBe(new Intl.Collator('de-u-kn-false-kf-upper-co-phonebk-hc-h12').resolvedOptions().locale, 'de-u-co-phonebk-kf-upper-kn-false');
