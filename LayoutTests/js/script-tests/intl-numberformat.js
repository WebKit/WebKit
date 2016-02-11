description("This test checks the behavior of Intl.NumberFormat as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

// 11.1 The Intl.NumberFormat Constructor

// The Intl.NumberFormat constructor is a standard built-in property of the Intl object.
shouldBeType("Intl.NumberFormat", "Function");

// 11.1.2 Intl.NumberFormat([ locales [, options]])
shouldBeType("Intl.NumberFormat()", "Intl.NumberFormat");
shouldBeType("Intl.NumberFormat.call({})", "Intl.NumberFormat");
shouldBeType("new Intl.NumberFormat()", "Intl.NumberFormat");

// Subclassable
var classPrefix = "class DerivedNumberFormat extends Intl.NumberFormat {};";
shouldBeTrue(classPrefix + "(new DerivedNumberFormat) instanceof DerivedNumberFormat");
shouldBeTrue(classPrefix + "(new DerivedNumberFormat) instanceof Intl.NumberFormat");
shouldBeTrue(classPrefix + "new DerivedNumberFormat().format(1) === '1'");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(new DerivedNumberFormat) === DerivedNumberFormat.prototype");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(Object.getPrototypeOf(new DerivedNumberFormat)) === Intl.NumberFormat.prototype");

function testNumberFormat(numberFormat, possibleDifferences) {
    var possibleOptions = possibleDifferences.map(function(difference) {
        var defaultOptions = {
            locale: undefined,
            numberingSystem: "latn",
            style: "decimal",
            currency: undefined,
            currencyDisplay: undefined,
            minimumIntegerDigits: 1,
            minimumFractionDigits: 0,
            maximumFractionDigits: 3,
            minimumSignificantDigits: undefined,
            maximumSignificantDigits: undefined,
            useGrouping: true
        }
        Object.assign(defaultOptions, difference);
        return JSON.stringify(defaultOptions);
    });
    var actualOptions = JSON.stringify(numberFormat.resolvedOptions())
    return possibleOptions.includes(actualOptions);
}

// Locale is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en'), [{locale: 'en'}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('eN-uS'), [{locale: 'en-US'}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat(['en', 'de']), [{locale: 'en'}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('de'), [{locale: 'de'}])");

// The "nu" key is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('zh-Hans-CN-u-nu-hanidec'), [{locale: 'zh-Hans-CN-u-nu-hanidec', numberingSystem: 'hanidec'}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('ZH-hans-cn-U-Nu-Hanidec'), [{locale: 'zh-Hans-CN-u-nu-hanidec', numberingSystem: 'hanidec'}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en-u-nu-abcd'), [{locale: 'en'}])");

// Ignores irrelevant extension keys.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('zh-Hans-CN-u-aa-aaaa-co-pinyin-nu-hanidec-bb-bbbb'), [{locale: 'zh-Hans-CN-u-nu-hanidec', numberingSystem: 'hanidec'}])");

// The option localeMatcher is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {localeMatcher: 'lookup'}), [{locale: 'en'}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {localeMatcher: 'best fit'}), [{locale: 'en'}])");
shouldThrow("Intl.NumberFormat('en', {localeMatcher: 'LookUp'})", '\'RangeError: localeMatcher must be either "lookup" or "best fit"\'');
shouldThrow("Intl.NumberFormat('en', { get localeMatcher() { throw 42; } })", "'42'");
shouldThrow("Intl.NumberFormat('en', {localeMatcher: {toString() { throw 42; }}})", "'42'");

// The option style is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'decimal'}), [{locale: 'en', style: 'decimal'}])");
shouldThrow("Intl.NumberFormat('en', {style: 'currency'})", "'TypeError: currency must be a string'");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'percent'}), [{locale: 'en', style: 'percent', maximumFractionDigits: 0}])");
shouldThrow("Intl.NumberFormat('en', {style: 'Decimal'})", '\'RangeError: style must be either "decimal", "percent", or "currency"\'');
shouldThrow("Intl.NumberFormat('en', { get style() { throw 42; } })", "'42'");

// The option currency is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'USD'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'symbol', minimumFractionDigits: 2, maximumFractionDigits: 2}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'UsD'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'symbol', minimumFractionDigits: 2, maximumFractionDigits: 2}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'CLF'}), [{locale: 'en', style: 'currency', currency: 'CLF', currencyDisplay: 'symbol', minimumFractionDigits: 4, maximumFractionDigits: 4}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'cLf'}), [{locale: 'en', style: 'currency', currency: 'CLF', currencyDisplay: 'symbol', minimumFractionDigits: 4, maximumFractionDigits: 4}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'XXX'}), [{locale: 'en', style: 'currency', currency: 'XXX', currencyDisplay: 'symbol', minimumFractionDigits: 2, maximumFractionDigits: 2}])");
shouldThrow("Intl.NumberFormat('en', {style: 'currency', currency: 'US$'})", "'RangeError: currency is not a well-formed currency code'");
shouldThrow("Intl.NumberFormat('en', {style: 'currency', currency: 'US'})", "'RangeError: currency is not a well-formed currency code'");
shouldThrow("Intl.NumberFormat('en', {style: 'currency', currency: 'US Dollar'})", "'RangeError: currency is not a well-formed currency code'");
shouldThrow("Intl.NumberFormat('en', {style: 'currency', get currency() { throw 42; }})", "'42'");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'decimal', currency: 'USD'}), [{locale: 'en', style: 'decimal'}])");

// The option currencyDisplay is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'code'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'code', minimumFractionDigits: 2, maximumFractionDigits: 2}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'symbol'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'symbol', minimumFractionDigits: 2, maximumFractionDigits: 2}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'name'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'name', minimumFractionDigits: 2, maximumFractionDigits: 2}])");
shouldThrow("Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'Code'})", '\'RangeError: currencyDisplay must be either "code", "symbol", or "name"\'');
shouldThrow("Intl.NumberFormat('en', {style: 'currency', currency: 'USD', get currencyDisplay() { throw 42; }})", "'42'");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'decimal', currencyDisplay: 'code'}), [{locale: 'en', style: 'decimal'}])");

// The option minimumIntegerDigits is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: 1}), [{locale: 'en', minimumIntegerDigits: 1}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: '2'}), [{locale: 'en', minimumIntegerDigits: 2}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: {valueOf() { return 3; }}}), [{locale: 'en', minimumIntegerDigits: 3}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: 4.9}), [{locale: 'en', minimumIntegerDigits: 4}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: 21}), [{locale: 'en', minimumIntegerDigits: 21}])");
shouldThrow("Intl.NumberFormat('en', {minimumIntegerDigits: 0})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {minimumIntegerDigits: 22})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {minimumIntegerDigits: 0.9})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {minimumIntegerDigits: 21.1})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {minimumIntegerDigits: NaN})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {minimumIntegerDigits: Infinity})", "'RangeError: minimumIntegerDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', { get minimumIntegerDigits() { throw 42; } })", "'42'");
shouldThrow("Intl.NumberFormat('en', {minimumIntegerDigits: {valueOf() { throw 42; }}})", "'42'");

// The option minimumFractionDigits is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {minimumFractionDigits: 0}), [{locale: 'en', minimumFractionDigits: 0, maximumFractionDigits: 3}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {style: 'percent', minimumFractionDigits: 0}), [{locale: 'en', style: 'percent', minimumFractionDigits: 0, maximumFractionDigits: 0}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {minimumFractionDigits: 6}), [{locale: 'en', minimumFractionDigits: 6, maximumFractionDigits: 6}])");
shouldThrow("Intl.NumberFormat('en', {minimumFractionDigits: -1})", "'RangeError: minimumFractionDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {minimumFractionDigits: 21})", "'RangeError: minimumFractionDigits is out of range'");

// The option maximumFractionDigits is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {maximumFractionDigits: 6}), [{locale: 'en', maximumFractionDigits: 6}])");
shouldThrow("Intl.NumberFormat('en', {minimumFractionDigits: 7, maximumFractionDigits: 6})", "'RangeError: maximumFractionDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {maximumFractionDigits: -1})", "'RangeError: maximumFractionDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {maximumFractionDigits: 21})", "'RangeError: maximumFractionDigits is out of range'");

// The option minimumSignificantDigits is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {minimumSignificantDigits: 6}), [{locale: 'en', minimumSignificantDigits: 6, maximumSignificantDigits: 21}])");
shouldThrow("Intl.NumberFormat('en', {minimumSignificantDigits: 0})", "'RangeError: minimumSignificantDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {minimumSignificantDigits: 22})", "'RangeError: minimumSignificantDigits is out of range'");

// The option maximumSignificantDigits is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {maximumSignificantDigits: 6}), [{locale: 'en', minimumSignificantDigits: 1, maximumSignificantDigits: 6}])");
shouldThrow("Intl.NumberFormat('en', {minimumSignificantDigits: 7, maximumSignificantDigits: 6})", "'RangeError: maximumSignificantDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {maximumSignificantDigits: 0})", "'RangeError: maximumSignificantDigits is out of range'");
shouldThrow("Intl.NumberFormat('en', {maximumSignificantDigits: 22})", "'RangeError: maximumSignificantDigits is out of range'");

// The option useGrouping is processed correctly.
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {useGrouping: true}), [{locale: 'en', useGrouping: true}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {useGrouping: false}), [{locale: 'en', useGrouping: false}])");
shouldBeTrue("testNumberFormat(Intl.NumberFormat('en', {useGrouping: 'false'}), [{locale: 'en', useGrouping: true}])");
shouldThrow("Intl.NumberFormat('en', { get useGrouping() { throw 42; } })", "'42'");

// 11.2 Properties of the Intl.NumberFormat Constructor

// length property (whose value is 0)
shouldBe("Intl.NumberFormat.length", "0");

// 11.2.1 Intl.NumberFormat.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.NumberFormat, 'prototype').writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.NumberFormat, 'prototype').enumerable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.NumberFormat, 'prototype').configurable");

// 11.2.2 Intl.NumberFormat.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe("Intl.NumberFormat.supportedLocalesOf.length", "1");

// Returns SupportedLocales
shouldBeType("Intl.NumberFormat.supportedLocalesOf()", "Array");
// Doesn't care about `this`.
shouldBe("Intl.NumberFormat.supportedLocalesOf.call(null, 'en')", "[ 'en' ]");
shouldBe("Intl.NumberFormat.supportedLocalesOf.call({}, 'en')", "[ 'en' ]");
shouldBe("Intl.NumberFormat.supportedLocalesOf.call(1, 'en')", "[ 'en' ]");
// Ignores non-object, non-string list.
shouldBe("Intl.NumberFormat.supportedLocalesOf(9)", "[]");
// Makes an array of tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf('en')", "[ 'en' ]");
// Handles array-like objects with holes.
shouldBe("Intl.NumberFormat.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })", "[ 'es', 'en', 'de' ]");
// Deduplicates tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])", "[ 'en', 'pt', 'es' ]");
// Canonicalizes tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf('En-laTn-us-variant2-variant1-1abc-U-ko-tRue-A-aa-aaa-x-RESERVED')", "[ 'en-Latn-US-variant2-variant1-1abc-a-aa-aaa-u-ko-true-x-reserved' ]");
// Replaces outdated tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf('no-bok')", "[ 'nb' ]");
// Doesn't throw, but ignores private tags.
shouldBe("Intl.NumberFormat.supportedLocalesOf('x-some-thing')", "[]");
// Throws on problems with length, get, or toString.
shouldThrow("Intl.NumberFormat.supportedLocalesOf(Object.create(null, { length: { get() { throw Error('a') } } }))", "'Error: a'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw Error('b') } } }))", "'Error: b'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf([ { toString() { throw Error('c') } } ])", "'Error: c'");
// Throws on bad tags.
shouldThrow("Intl.NumberFormat.supportedLocalesOf([ 5 ])", "'TypeError: locale value must be a string or object'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('')", "'RangeError: invalid language tag: '");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('a')", "'RangeError: invalid language tag: a'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('abcdefghij')", "'RangeError: invalid language tag: abcdefghij'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('#$')", "'RangeError: invalid language tag: #$'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-@-abc')", "'RangeError: invalid language tag: en-@-abc'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-u')", "'RangeError: invalid language tag: en-u'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-u-kn-true-u-ko-true')", "'RangeError: invalid language tag: en-u-kn-true-u-ko-true'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-x')", "'RangeError: invalid language tag: en-x'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-*')", "'RangeError: invalid language tag: en-*'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en-')", "'RangeError: invalid language tag: en-'");
shouldThrow("Intl.NumberFormat.supportedLocalesOf('en--US')", "'RangeError: invalid language tag: en--US'");

// 11.3 Properties of the Intl.NumberFormat Prototype Object

// The value of Intl.NumberFormat.prototype.constructor is %NumberFormat%.
shouldBe("Intl.NumberFormat.prototype.constructor", "Intl.NumberFormat");

// 11.3.3 Intl.NumberFormat.prototype.format

// This named accessor property returns a function that formats a number according to the effective locale and the formatting options of this NumberFormat object.
shouldBeType("Intl.NumberFormat.prototype.format", "Function");

// The value of the [[Get]] attribute is a function
shouldBeType("Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').get", "Function");

// The value of the [[Set]] attribute is undefined.
shouldBe("Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').set", "undefined");

// Match Firefox where unspecifed.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').configurable");

// The value of F’s length property is 1.
shouldBe("Intl.NumberFormat.prototype.format.length", "1");

// Throws on non-NumberFormat this.
shouldThrow("Object.defineProperty({}, 'format', Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format')).format", "'TypeError: Intl.NumberFormat.prototype.format called on value that\\'s not an object initialized as a NumberFormat'");

// The format function is unique per instance.
shouldBeTrue("Intl.NumberFormat.prototype.format !== Intl.NumberFormat().format");
shouldBeTrue("new Intl.NumberFormat().format !== new Intl.NumberFormat().format");

// 11.3.4 Format Number Functions

// 1. Let nf be the this value.
// 2. Assert: Type(nf) is Object and nf has an [[initializedNumberFormat]] internal slot whose value is true.
// This should not be reachable, since format is bound to an initialized numberformat.

// 3. If value is not provided, let value be undefined.
// 4. Let x be ToNumber(value).
// 5. ReturnIfAbrupt(x).
shouldThrow("Intl.NumberFormat.prototype.format({ valueOf() { throw Error('5') } })", "'Error: 5'");

// Format is bound, so calling with alternate "this" has no effect.
shouldBe("Intl.NumberFormat.prototype.format.call(null, 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("Intl.NumberFormat.prototype.format.call(Intl.DateTimeFormat('ar'), 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("Intl.NumberFormat.prototype.format.call(5, 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("new Intl.NumberFormat().format.call(null, 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("new Intl.NumberFormat().format.call(Intl.DateTimeFormat('ar'), 1.2)", "Intl.NumberFormat().format(1.2)");
shouldBe("new Intl.NumberFormat().format.call(5, 1.2)", "Intl.NumberFormat().format(1.2)");

// 11.3.5 Intl.NumberFormat.prototype.resolvedOptions ()

shouldBe("Intl.NumberFormat.prototype.resolvedOptions.length", "0");

// Returns a new object whose properties and attributes are set as if constructed by an object literal.
shouldBeType("Intl.NumberFormat.prototype.resolvedOptions()", "Object");

// Returns a new object each time.
shouldBeFalse("Intl.NumberFormat.prototype.resolvedOptions() === Intl.NumberFormat.prototype.resolvedOptions()");

// Throws on non-NumberFormat this.
shouldThrow("Intl.NumberFormat.prototype.resolvedOptions.call(5)", "'TypeError: Intl.NumberFormat.prototype.resolvedOptions called on value that\\'s not an object initialized as a NumberFormat'");

// Returns the default options.
shouldBe("var options = Intl.NumberFormat.prototype.resolvedOptions(); delete options['locale']; JSON.stringify(options)", '\'{"numberingSystem":"latn","style":"decimal","minimumIntegerDigits":1,"minimumFractionDigits":0,"maximumFractionDigits":3,"useGrouping":true}\'');
