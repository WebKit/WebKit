description("This test checks the behavior of Intl.Collator as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

// 10.1 The Intl.Collator Constructor

// The Intl.Collator constructor is a standard built-in property of the Intl object.
shouldBeType("Intl.Collator", "Function");

// 10.1.2 Intl.Collator([ locales [, options]])
shouldBeType("Intl.Collator()", "Intl.Collator");
shouldBeType("Intl.Collator.call({})", "Intl.Collator");
shouldBeType("new Intl.Collator()", "Intl.Collator");
shouldBeType("Intl.Collator('en')", "Intl.Collator");
shouldBeType("Intl.Collator(42)", "Intl.Collator");
shouldThrow("Intl.Collator(null)", "'TypeError: null is not an object (evaluating \\'Intl.Collator(null)\\')'");
shouldThrow("Intl.Collator({ get length() { throw 42; } })", "'42'");
shouldBeType("Intl.Collator('en', { })", "Intl.Collator");
shouldBeType("Intl.Collator('en', 42)", "Intl.Collator");
shouldThrow("Intl.Collator('en', null)", "'TypeError: null is not an object (evaluating \\'Intl.Collator(\\'en\\', null)\\')'");

// Subclassable
var classPrefix = "class DerivedCollator extends Intl.Collator {};";
shouldBeTrue(classPrefix + "(new DerivedCollator) instanceof DerivedCollator");
shouldBeTrue(classPrefix + "(new DerivedCollator) instanceof Intl.Collator");
shouldBeTrue(classPrefix + "new DerivedCollator().compare('a', 'b') === -1");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(new DerivedCollator) === DerivedCollator.prototype");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(Object.getPrototypeOf(new DerivedCollator)) === Intl.Collator.prototype");

var testCollator = function(collator, possibleOptionDifferences) {
    var possibleOptions = possibleOptionDifferences.map(function(difference) {
        var defaultOptions = {
            locale: undefined,
            usage: "sort",
            sensitivity: "variant",
            ignorePunctuation: false,
            collation: "default",
            numeric: false
        }
        Object.assign(defaultOptions, difference);
        return JSON.stringify(defaultOptions);
    });
    var actualOptions = JSON.stringify(collator.resolvedOptions())
    return possibleOptions.includes(actualOptions);
}

// Locale is processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en'), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('eN-uS'), [{locale: 'en-US'}])");
shouldBeTrue("testCollator(Intl.Collator(['en', 'de']), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('de'), [{locale: 'de'}])");

// The "co" key is processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en-u-co-eor'), [{locale: 'en-u-co-eor', collation: 'eor'}, {locale: 'en'}])");
shouldBeTrue("testCollator(new Intl.Collator('en-u-co-eor'), [{locale: 'en-u-co-eor', collation: 'eor'}, {locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('En-U-Co-Eor'), [{locale: 'en-u-co-eor', collation: 'eor'}, {locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-co-phonebk'), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-co-standard'), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-co-search'), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-co-abcd'), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('de-u-co-phonebk'), [{locale: 'de-u-co-phonebk', collation: 'phonebk'}, {locale: 'de'}])");

// The "kn" key is processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en-u-kn'), [{locale: 'en', numeric: true}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-kn-true'), [{locale: 'en-u-kn-true', numeric: true}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-kn-false'), [{locale: 'en-u-kn-false', numeric: false}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-kn-abcd'), [{locale: 'en'}])");

// Ignores irrelevant extension keys.
shouldBeTrue("testCollator(Intl.Collator('en-u-aa-aaaa-kn-true-bb-bbbb-co-eor-cc-cccc-y-yyd'), [{locale: 'en-u-co-eor-kn-true', collation: 'eor', numeric: true}, {locale: 'en-u-kn-true', numeric: true}])");

// Ignores other extensions.
shouldBeTrue("testCollator(Intl.Collator('en-u-kn-true-a-aa'), [{locale: 'en-u-kn-true', numeric: true}])");
shouldBeTrue("testCollator(Intl.Collator('en-a-aa-u-kn-true'), [{locale: 'en-u-kn-true', numeric: true}])");
shouldBeTrue("testCollator(Intl.Collator('en-a-aa-u-kn-true-b-bb'), [{locale: 'en-u-kn-true', numeric: true}])");

// The option usage is processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en', {usage: 'sort'}), [{locale: 'en', usage: 'sort'}])");
shouldBeTrue("testCollator(Intl.Collator('en', {usage: 'search'}), [{locale: 'en', usage: 'search'}])");
shouldThrow("Intl.Collator('en', {usage: 'Sort'})", '\'RangeError: usage must be either "sort" or "search"\'');
shouldThrow("Intl.Collator('en', { get usage() { throw 42; } })", "'42'");
shouldThrow("Intl.Collator('en', {usage: {toString() { throw 42; }}})", "'42'");

// The option localeMatcher is processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en', {localeMatcher: 'lookup'}), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('en', {localeMatcher: 'best fit'}), [{locale: 'en'}])");
shouldThrow("Intl.Collator('en', {localeMatcher: 'LookUp'})", '\'RangeError: localeMatcher must be either "lookup" or "best fit"\'');
shouldThrow("Intl.Collator('en', { get localeMatcher() { throw 42; } })", "'42'");

// The option numeric is processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en', {numeric: true}), [{locale: 'en', numeric: true}])");
shouldBeTrue("testCollator(Intl.Collator('en', {numeric: false}), [{locale: 'en', numeric: false}])");
shouldBeTrue("testCollator(Intl.Collator('en', {numeric: 'false'}), [{locale: 'en', numeric: true}])");
shouldBeTrue("testCollator(Intl.Collator('en', {numeric: { }}), [{locale: 'en', numeric: true}])");
shouldThrow("Intl.Collator('en', { get numeric() { throw 42; } })", "'42'");

// The option caseFirst is processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en', {caseFirst: 'upper'}), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('en', {caseFirst: 'lower'}), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('en', {caseFirst: 'false'}), [{locale: 'en'}])");
shouldBeTrue("testCollator(Intl.Collator('en', {caseFirst: false}), [{locale: 'en'}])");
shouldThrow("Intl.Collator('en', {caseFirst: 'true'})", '\'RangeError: caseFirst must be either "upper", "lower", or "false"\'');
shouldThrow("Intl.Collator('en', { get caseFirst() { throw 42; } })", "'42'");

// The option sensitivity is processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en', {sensitivity: 'base'}), [{locale: 'en', sensitivity: 'base'}])");
shouldBeTrue("testCollator(Intl.Collator('en', {sensitivity: 'accent'}), [{locale: 'en', sensitivity: 'accent'}])");
shouldBeTrue("testCollator(Intl.Collator('en', {sensitivity: 'case'}), [{locale: 'en', sensitivity: 'case'}])");
shouldBeTrue("testCollator(Intl.Collator('en', {sensitivity: 'variant'}), [{locale: 'en', sensitivity: 'variant'}])");
shouldThrow("Intl.Collator('en', {sensitivity: 'abcd'})", '\'RangeError: sensitivity must be either "base", "accent", "case", or "variant"\'');
shouldThrow("Intl.Collator('en', { get sensitivity() { throw 42; } })", "'42'");

// The option ignorePunctuation is processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en', {ignorePunctuation: true}), [{locale: 'en', ignorePunctuation: true}])");
shouldBeTrue("testCollator(Intl.Collator('en', {ignorePunctuation: false}), [{locale: 'en', ignorePunctuation: false}])");
shouldBeTrue("testCollator(Intl.Collator('en', {ignorePunctuation: 'false'}), [{locale: 'en', ignorePunctuation: true}])");
shouldThrow("Intl.Collator('en', { get ignorePunctuation() { throw 42; } })", "'42'");

// Options override the language tag.
shouldBeTrue("testCollator(Intl.Collator('en-u-kn-true', {numeric: false}), [{locale: 'en', numeric: false}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-kn-false', {numeric: true}), [{locale: 'en', numeric: true}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-kn-true', {numeric: true}), [{locale: 'en-u-kn-true', numeric: true}])");
shouldBeTrue("testCollator(Intl.Collator('en-u-kn-false', {numeric: false}), [{locale: 'en-u-kn-false', numeric: false}])");

// Options and extension keys are processed correctly.
shouldBeTrue("testCollator(Intl.Collator('en-a-aa-u-kn-false-co-eor-b-bb', {usage: 'sort', numeric: true, caseFirst: 'lower', sensitivity: 'case', ignorePunctuation: true}), [{locale: 'en-u-co-eor', usage: 'sort', sensitivity: 'case', ignorePunctuation: true, collation: 'eor', numeric: true}, {locale: 'en', usage: 'sort', sensitivity: 'case', ignorePunctuation: true, numeric: true}])");

// 10.2 Properties of the Intl.Collator Constructor

// length property (whose value is 0)
shouldBe("Intl.Collator.length", "0");

// 10.2.1 Intl.Collator.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.Collator, 'prototype').writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.Collator, 'prototype').enumerable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.Collator, 'prototype').configurable");

// 10.2.2 Intl.Collator.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe("Intl.Collator.supportedLocalesOf.length", "1");

// Returns SupportedLocales.
shouldBeType("Intl.Collator.supportedLocalesOf()", "Array");
// Doesn't care about `this`.
shouldBe("Intl.Collator.supportedLocalesOf.call(null, 'en')", "[ 'en' ]");
shouldBe("Intl.Collator.supportedLocalesOf.call({}, 'en')", "[ 'en' ]");
shouldBe("Intl.Collator.supportedLocalesOf.call(1, 'en')", "[ 'en' ]");
// Ignores non-object, non-string list.
shouldBe("Intl.Collator.supportedLocalesOf(9)", "[]");
// Makes an array of tags.
shouldBe("Intl.Collator.supportedLocalesOf('en')", "[ 'en' ]");
// Handles array-like objects with holes.
shouldBe("Intl.Collator.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })", "[ 'es', 'en', 'de' ]");
// Deduplicates tags.
shouldBe("Intl.Collator.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])", "[ 'en', 'pt', 'es' ]");
// Canonicalizes tags.
shouldBe("Intl.Collator.supportedLocalesOf('En-laTn-us-variant2-variant1-1abc-U-ko-tRue-A-aa-aaa-x-RESERVED')", "[ 'en-Latn-US-variant2-variant1-1abc-a-aa-aaa-u-ko-true-x-reserved' ]");
// Replaces outdated tags.
shouldBe("Intl.Collator.supportedLocalesOf('no-bok')", "[ 'nb' ]");
// Doesn't throw, but ignores private tags.
shouldBe("Intl.Collator.supportedLocalesOf('x-some-thing')", "[]");
// Throws on problems with length, get, or toString.
shouldThrow("Intl.Collator.supportedLocalesOf(Object.create(null, { length: { get() { throw Error('a') } } }))", "'Error: a'");
shouldThrow("Intl.Collator.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw Error('b') } } }))", "'Error: b'");
shouldThrow("Intl.Collator.supportedLocalesOf([ { toString() { throw Error('c') } } ])", "'Error: c'");
// Throws on bad tags.
shouldThrow("Intl.Collator.supportedLocalesOf([ 5 ])", "'TypeError: locale value must be a string or object'");
shouldThrow("Intl.Collator.supportedLocalesOf('')", "'RangeError: invalid language tag: '");
shouldThrow("Intl.Collator.supportedLocalesOf('a')", "'RangeError: invalid language tag: a'");
shouldThrow("Intl.Collator.supportedLocalesOf('abcdefghij')", "'RangeError: invalid language tag: abcdefghij'");
shouldThrow("Intl.Collator.supportedLocalesOf('#$')", "'RangeError: invalid language tag: #$'");
shouldThrow("Intl.Collator.supportedLocalesOf('en-@-abc')", "'RangeError: invalid language tag: en-@-abc'");
shouldThrow("Intl.Collator.supportedLocalesOf('en-u')", "'RangeError: invalid language tag: en-u'");
shouldThrow("Intl.Collator.supportedLocalesOf('en-u-kn-true-u-ko-true')", "'RangeError: invalid language tag: en-u-kn-true-u-ko-true'");
shouldThrow("Intl.Collator.supportedLocalesOf('en-x')", "'RangeError: invalid language tag: en-x'");
shouldThrow("Intl.Collator.supportedLocalesOf('en-*')", "'RangeError: invalid language tag: en-*'");
shouldThrow("Intl.Collator.supportedLocalesOf('en-')", "'RangeError: invalid language tag: en-'");
shouldThrow("Intl.Collator.supportedLocalesOf('en--US')", "'RangeError: invalid language tag: en--US'");

// 10.3 Properties of the Intl.Collator Prototype Object

// The value of Intl.Collator.prototype.constructor is %Collator%.
shouldBe("Intl.Collator.prototype.constructor", "Intl.Collator");

// 10.3.3 Intl.Collator.prototype.compare

// This named accessor property returns a function that compares two strings according to the sort order of this Collator object.
shouldBeType("Intl.Collator.prototype.compare", "Function");

// The value of the [[Get]] attribute is a function
shouldBeType("Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').get", "Function");

// The value of the [[Set]] attribute is undefined.
shouldBe("Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').set", "undefined");

// Match Firefox where unspecifed.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare').configurable");

// The value of F’s length property is 2.
shouldBe("Intl.Collator.prototype.compare.length", "2");

// Throws on non-Collator this.
shouldThrow("Object.defineProperty({}, 'compare', Object.getOwnPropertyDescriptor(Intl.Collator.prototype, 'compare')).compare", "'TypeError: Intl.Collator.prototype.compare called on value that\\'s not an object initialized as a Collator'");

// The compare function is unique per instance.
shouldBeTrue("Intl.Collator.prototype.compare !== Intl.Collator().compare");
shouldBeTrue("new Intl.Collator().compare !== new Intl.Collator().compare");

// 10.3.4 Collator Compare Functions

// 1. Let collator be the this value.
// 2. Assert: Type(collator) is Object and collator has an [[initializedCollator]] internal slot whose value is true.
// This should not be reachable, since compare is bound to an initialized collator.

// 3. If x is not provided, let x be undefined.
// 4. If y is not provided, let y be undefined.
// 5. Let X be ToString(x).
// 6. ReturnIfAbrupt(X).
var badCalls = 0;
shouldThrow("Intl.Collator.prototype.compare({ toString() { throw Error('6') } }, { toString() { ++badCalls; return ''; } })", "'Error: 6'");
shouldBe("badCalls", "0");

// 7. Let Y be ToString(y).
// 8. ReturnIfAbrupt(Y).
shouldThrow("Intl.Collator.prototype.compare('a', { toString() { throw Error('8') } })", "'Error: 8'");

// Compare is bound, so calling with alternate "this" has no effect.
shouldBe("Intl.Collator.prototype.compare.call(null, 'a', 'b')", "-1");
shouldBe("Intl.Collator.prototype.compare.call(Intl.Collator('en', { sensitivity:'base' }), 'A', 'a')", "1");
shouldBe("Intl.Collator.prototype.compare.call(5, 'a', 'b')", "-1");
shouldBe("new Intl.Collator().compare.call(null, 'a', 'b')", "-1");
shouldBe("new Intl.Collator().compare.call(Intl.Collator('en', { sensitivity:'base' }), 'A', 'a')", "1");
shouldBe("new Intl.Collator().compare.call(5, 'a', 'b')", "-1");

// Test comparing undefineds.
shouldBe("Intl.Collator.prototype.compare()", "0");
shouldBe("Intl.Collator.prototype.compare('undefinec')", "-1");
shouldBe("Intl.Collator.prototype.compare('undefinee')", "1");

// Test locales.
shouldBe("Intl.Collator('en').compare('ä', 'z')", "-1");
shouldBe("Intl.Collator('sv').compare('ä', 'z')", "1");

// Test collations.
shouldBe("Intl.Collator('de').compare('ö', 'od')", "-1");
shouldBeTrue("Intl.Collator('de-u-co-phonebk').resolvedOptions().collation != 'phonebk' || Intl.Collator('de-u-co-phonebk').compare('ö', 'od') == 1");

// Test the usage option.
var sortCompare = Intl.Collator("en", {usage: "sort"}).compare;
shouldBe("JSON.stringify(['A', 'B', 'C', 'a', 'b', 'c'].sort(sortCompare))", '\'["a","A","b","B","c","C"]\'');

var searchCompare = Intl.Collator("en", {usage: "search", sensitivity: "base"}).compare;
shouldBe("JSON.stringify(['A', 'B', 'C', 'a', 'b', 'c'].filter(x => (searchCompare(x, 'b') == 0)))", '\'["B","b"]\'');

// Test the sensitivity option.
var baseCompare = Intl.Collator("en", {sensitivity: "base"}).compare;
shouldBe("baseCompare('a', 'b')", "-1");
shouldBe("baseCompare('a', 'ä')", "0");
shouldBe("baseCompare('a', 'A')", "0");
shouldBe("baseCompare('a', 'ⓐ')", "0");

var accentCompare = Intl.Collator("en", {sensitivity: "accent"}).compare;
shouldBe("accentCompare('a', 'b')", "-1");
shouldBe("accentCompare('a', 'ä')", "-1");
shouldBe("accentCompare('a', 'A')", "0");
shouldBe("accentCompare('a', 'ⓐ')", "0");

var caseCompare = Intl.Collator("en", {sensitivity: "case"}).compare;
shouldBe("caseCompare('a', 'b')", "-1");
shouldBe("caseCompare('a', 'ä')", "0");
shouldBe("caseCompare('a', 'A')", "-1");
shouldBe("caseCompare('a', 'ⓐ')", "0");

var variantCompare = Intl.Collator("en", {sensitivity: "variant"}).compare;
shouldBe("variantCompare('a', 'b')", "-1");
shouldBe("variantCompare('a', 'ä')", "-1");
shouldBe("variantCompare('a', 'A')", "-1");
shouldBe("variantCompare('a', 'ⓐ')", "-1");

// Test the numeric option.
var nonNumericCompare = Intl.Collator("en", {numeric: false}).compare;
shouldBe("nonNumericCompare('1', '2')", "-1");
shouldBe("nonNumericCompare('2', '10')", "1");
shouldBe("nonNumericCompare('01', '1')", "-1");
shouldBe("nonNumericCompare('๑', '๒')", "-1");
shouldBe("nonNumericCompare('๒', '๑๐')", "1");
shouldBe("nonNumericCompare('๐๑', '๑')", "-1");

var numericCompare = Intl.Collator("en", {numeric: true}).compare;
shouldBe("numericCompare('1', '2')", "-1");
shouldBe("numericCompare('2', '10')", "-1");
shouldBe("numericCompare('01', '1')", "0");
shouldBe("numericCompare('๑', '๒')", "-1");
shouldBe("numericCompare('๒', '๑๐')", "-1");
shouldBe("numericCompare('๐๑', '๑')", "0");

// Test the caseFirst option.
// FIXME: The result of Intl.Collator('en', {caseFirst: 'upper'}).compare('a', 'A') should be 1.
shouldBe("Intl.Collator('en', {caseFirst: 'upper'}).compare('a', 'A')", "-1");
shouldBe("Intl.Collator('en', {caseFirst: 'lower'}).compare('a', 'A')", "-1");
shouldBe("Intl.Collator('en', {caseFirst: 'false'}).compare('a', 'A')", "-1");

// Test the ignorePunctuation option.
var notIgnorePunctuationCompare = Intl.Collator("en", {ignorePunctuation: false}).compare;
var ignorePunctuationCompare = Intl.Collator("en", {ignorePunctuation: true}).compare;
var punctuations = ["\\'", "\"", ":", ";", ",", "-", "!", ".", "?"];
for (let punctuation of punctuations) {
    shouldBe("notIgnorePunctuationCompare('ab', 'a" + punctuation + "a')", "1");
    shouldBe("notIgnorePunctuationCompare('ab', 'a" + punctuation + "b')", "1");
    shouldBe("notIgnorePunctuationCompare('ab', 'a" + punctuation + "c')", "1");

    shouldBe("ignorePunctuationCompare('ab', 'a" + punctuation + "a')", "1");
    shouldBe("ignorePunctuationCompare('ab', 'a" + punctuation + "b')", "0");
    shouldBe("ignorePunctuationCompare('ab', 'a" + punctuation + "c')", "-1");
}

// FIXME: The result of ignorePunctuationCompare('ab', 'a b') should be 1. The whitespace shouldn't be ignored.
shouldBe("ignorePunctuationCompare('ab', 'a b')", "0");

// Returns 0 for strings that are canonically equivalent.
shouldBe("Intl.Collator('en').compare('A\u0308', '\u00c4')", "0"); // A + umlaut == A-umlaut.
shouldBe("Intl.Collator('en').compare('A\u0327\u030a', '\u212b\u0327')", "0"); // A + cedilla + ring == A-ring + cedilla.

// 10.3.5 Intl.Collator.prototype.resolvedOptions ()

shouldBe("Intl.Collator.prototype.resolvedOptions.length", "0");

// Returns a new object whose properties and attributes are set as if constructed by an object literal.
shouldBeType("Intl.Collator.prototype.resolvedOptions()", "Object");

// Returns a new object each time.
shouldBeFalse("Intl.Collator.prototype.resolvedOptions() === Intl.Collator.prototype.resolvedOptions()");

// Throws on non-Collator this.
shouldThrow("Intl.Collator.prototype.resolvedOptions.call(5)", "'TypeError: Intl.Collator.prototype.resolvedOptions called on value that\\'s not an object initialized as a Collator'");

// Returns the default options.
shouldBe("var options = Intl.Collator.prototype.resolvedOptions(); delete options['locale']; JSON.stringify(options)", '\'{"usage":"sort","sensitivity":"variant","ignorePunctuation":false,"collation":"default","numeric":false}\'');
