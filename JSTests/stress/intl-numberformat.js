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

// 11.1 The Intl.NumberFormat Constructor

// The Intl.NumberFormat constructor is a standard built-in property of the Intl object.
shouldBe(Intl.NumberFormat instanceof Function, true);

// 11.1.2 Intl.NumberFormat([ locales [, options]])
shouldBe(Intl.NumberFormat() instanceof Intl.NumberFormat, true);
shouldBe(Intl.NumberFormat.call({}) instanceof Intl.NumberFormat, true);
shouldBe(new Intl.NumberFormat() instanceof Intl.NumberFormat, true);

// Subclassable
{
    class DerivedNumberFormat extends Intl.NumberFormat {}
    shouldBe((new DerivedNumberFormat) instanceof DerivedNumberFormat, true);
    shouldBe((new DerivedNumberFormat) instanceof Intl.NumberFormat, true);
    shouldBe(new DerivedNumberFormat('en').format(1), '1');
    shouldBe(Object.getPrototypeOf(new DerivedNumberFormat), DerivedNumberFormat.prototype);
    shouldBe(Object.getPrototypeOf(Object.getPrototypeOf(new DerivedNumberFormat)), Intl.NumberFormat.prototype);
}

function testNumberFormat(numberFormat, possibleDifferences) {
    var possibleOptions = possibleDifferences.map(function(difference) {
        var defaultOptions = {
            locale: undefined,
            numberingSystem: "latn",
            style: "decimal",
            currency: undefined,
            currencyDisplay: undefined,
            currencySign: undefined,
            minimumIntegerDigits: 1,
            minimumFractionDigits: 0,
            maximumFractionDigits: 3,
            minimumSignificantDigits: undefined,
            maximumSignificantDigits: undefined,
            useGrouping: true,
            notation: "standard",
            signDisplay: "auto"
        };
        Object.assign(defaultOptions, difference);
        return JSON.stringify(defaultOptions);
    });
    var actualOptions = JSON.stringify(numberFormat.resolvedOptions());
    return possibleOptions.includes(actualOptions);
}

// Locale is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en'), [{locale: 'en'}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('eN-uS'), [{locale: 'en-US'}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat(['en', 'de']), [{locale: 'en'}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('de'), [{locale: 'de'}]), true);

// The 'nu' key is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('zh-Hans-CN-u-nu-hanidec'), [{locale: 'zh-Hans-CN-u-nu-hanidec', numberingSystem: 'hanidec'}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('ZH-hans-cn-U-Nu-Hanidec'), [{locale: 'zh-Hans-CN-u-nu-hanidec', numberingSystem: 'hanidec'}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en-u-nu-abcd'), [{locale: 'en'}]), true);

let numberingSystems = [
    'arab', 'arabext', 'bali', 'beng', 'deva', 'fullwide', 'gujr', 'guru',
    'hanidec', 'khmr', 'knda', 'laoo', 'latn', 'limb', 'mlym', 'mong', 'mymr',
    'orya', 'tamldec', 'telu', 'thai', 'tibt'
];
for (let numberingSystem of numberingSystems)
    shouldBe(testNumberFormat(Intl.NumberFormat(`en-u-nu-${numberingSystem}`), [{locale: `en-u-nu-${numberingSystem}`, numberingSystem}]), true);

// Numbering system sensitive format().
shouldBe(Intl.NumberFormat('en-u-nu-arab').format(1234567890), '١٬٢٣٤٬٥٦٧٬٨٩٠');
shouldBe(Intl.NumberFormat('en-u-nu-arabext').format(1234567890), '۱٬۲۳۴٬۵۶۷٬۸۹۰');
shouldBe(Intl.NumberFormat('en-u-nu-bali').format(1234567890), '᭑,᭒᭓᭔,᭕᭖᭗,᭘᭙᭐');
shouldBe(Intl.NumberFormat('en-u-nu-beng').format(1234567890), '১,২৩৪,৫৬৭,৮৯০');
shouldBe(Intl.NumberFormat('en-u-nu-deva').format(1234567890), '१,२३४,५६७,८९०');
shouldBe(Intl.NumberFormat('en-u-nu-fullwide').format(1234567890), '１,２３４,５６７,８９０');
shouldBe(Intl.NumberFormat('en-u-nu-gujr').format(1234567890), '૧,૨૩૪,૫૬૭,૮૯૦');
shouldBe(Intl.NumberFormat('en-u-nu-guru').format(1234567890), '੧,੨੩੪,੫੬੭,੮੯੦');
shouldBe(Intl.NumberFormat('en-u-nu-hanidec').format(1234567890), '一,二三四,五六七,八九〇');
shouldBe(Intl.NumberFormat('en-u-nu-khmr').format(1234567890), '១,២៣៤,៥៦៧,៨៩០');
shouldBe(Intl.NumberFormat('en-u-nu-knda').format(1234567890), '೧,೨೩೪,೫೬೭,೮೯೦');
shouldBe(Intl.NumberFormat('en-u-nu-laoo').format(1234567890), '໑,໒໓໔,໕໖໗,໘໙໐');
shouldBe(Intl.NumberFormat('en-u-nu-latn').format(1234567890), '1,234,567,890');
shouldBe(Intl.NumberFormat('en-u-nu-limb').format(1234567890), '᥇,᥈᥉᥊,᥋᥌᥍,᥎᥏᥆');
shouldBe(Intl.NumberFormat('en-u-nu-mlym').format(1234567890), '൧,൨൩൪,൫൬൭,൮൯൦');
shouldBe(Intl.NumberFormat('en-u-nu-mong').format(1234567890), '᠑,᠒᠓᠔,᠕᠖᠗,᠘᠙᠐');
shouldBe(Intl.NumberFormat('en-u-nu-mymr').format(1234567890), '၁,၂၃၄,၅၆၇,၈၉၀');
shouldBe(Intl.NumberFormat('en-u-nu-orya').format(1234567890), '୧,୨୩୪,୫୬୭,୮୯୦');
shouldBe(Intl.NumberFormat('en-u-nu-tamldec').format(1234567890), '௧,௨௩௪,௫௬௭,௮௯௦');
shouldBe(Intl.NumberFormat('en-u-nu-telu').format(1234567890), '౧,౨౩౪,౫౬౭,౮౯౦');
shouldBe(Intl.NumberFormat('en-u-nu-thai').format(1234567890), '๑,๒๓๔,๕๖๗,๘๙๐');
shouldBe(Intl.NumberFormat('en-u-nu-tibt').format(1234567890), '༡,༢༣༤,༥༦༧,༨༩༠');

// Ignores irrelevant extension keys.
shouldBe(testNumberFormat(Intl.NumberFormat('zh-Hans-CN-u-aa-aaaa-co-pinyin-nu-hanidec-bb-bbbb'), [{locale: 'zh-Hans-CN-u-nu-hanidec', numberingSystem: 'hanidec'}]), true);

// The option localeMatcher is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {localeMatcher: 'lookup'}), [{locale: 'en'}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {localeMatcher: 'best fit'}), [{locale: 'en'}]), true);
shouldThrow(() => Intl.NumberFormat('en', {localeMatcher: 'LookUp'}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', { get localeMatcher() { throw new Error(); } }), Error);
shouldThrow(() => Intl.NumberFormat('en', {localeMatcher: {toString() { throw new Error(); }}}), Error);

// The option style is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'decimal'}), [{locale: 'en', style: 'decimal'}]), true);
shouldThrow(() => Intl.NumberFormat('en', {style: 'currency'}), TypeError);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'percent'}), [{locale: 'en', style: 'percent', maximumFractionDigits: 0}]), true);
shouldThrow(() => Intl.NumberFormat('en', {style: 'Decimal'}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', { get style() { throw new Error(); } }), Error);

// The option currency is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'USD'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'symbol', currencySign: 'standard', minimumFractionDigits: 2, maximumFractionDigits: 2}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'UsD'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'symbol', currencySign: 'standard', minimumFractionDigits: 2, maximumFractionDigits: 2}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'CLF'}), [{locale: 'en', style: 'currency', currency: 'CLF', currencyDisplay: 'symbol', currencySign: 'standard', minimumFractionDigits: 4, maximumFractionDigits: 4}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'cLf'}), [{locale: 'en', style: 'currency', currency: 'CLF', currencyDisplay: 'symbol', currencySign: 'standard', minimumFractionDigits: 4, maximumFractionDigits: 4}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'XXX'}), [{locale: 'en', style: 'currency', currency: 'XXX', currencyDisplay: 'symbol', currencySign: 'standard', minimumFractionDigits: 2, maximumFractionDigits: 2}]), true);
shouldThrow(() => Intl.NumberFormat('en', {style: 'currency', currency: 'US$'}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {style: 'currency', currency: 'US'}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {style: 'currency', currency: 'US Dollar'}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {style: 'currency', get currency() { throw new Error(); }}), Error);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'decimal', currency: 'USD'}), [{locale: 'en', style: 'decimal'}]), true);

// The option currencyDisplay is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'code'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'code', currencySign: 'standard', minimumFractionDigits: 2, maximumFractionDigits: 2}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'symbol'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'symbol', currencySign: 'standard', minimumFractionDigits: 2, maximumFractionDigits: 2}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'name'}), [{locale: 'en', style: 'currency', currency: 'USD', currencyDisplay: 'name', currencySign: 'standard', minimumFractionDigits: 2, maximumFractionDigits: 2}]), true);
shouldThrow(() => Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'Code'}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {style: 'currency', currency: 'USD', get currencyDisplay() { throw new Error(); }}), Error);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'decimal', currencyDisplay: 'code'}), [{locale: 'en', style: 'decimal'}]), true);

// The option minimumIntegerDigits is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: 1}), [{locale: 'en', minimumIntegerDigits: 1}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: '2'}), [{locale: 'en', minimumIntegerDigits: 2}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: {valueOf() { return 3; }}}), [{locale: 'en', minimumIntegerDigits: 3}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: 4.9}), [{locale: 'en', minimumIntegerDigits: 4}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {minimumIntegerDigits: 21}), [{locale: 'en', minimumIntegerDigits: 21}]), true);
shouldThrow(() => Intl.NumberFormat('en', {minimumIntegerDigits: 0}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {minimumIntegerDigits: 22}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {minimumIntegerDigits: 0.9}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {minimumIntegerDigits: 21.1}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {minimumIntegerDigits: NaN}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {minimumIntegerDigits: Infinity}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', { get minimumIntegerDigits() { throw new Error(); } }), Error);
shouldThrow(() => Intl.NumberFormat('en', {minimumIntegerDigits: {valueOf() { throw new Error(); }}}), Error);

// The option minimumFractionDigits is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {minimumFractionDigits: 0}), [{locale: 'en', minimumFractionDigits: 0, maximumFractionDigits: 3}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {style: 'percent', minimumFractionDigits: 0}), [{locale: 'en', style: 'percent', minimumFractionDigits: 0, maximumFractionDigits: 0}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {minimumFractionDigits: 6}), [{locale: 'en', minimumFractionDigits: 6, maximumFractionDigits: 6}]), true);
shouldThrow(() => Intl.NumberFormat('en', {minimumFractionDigits: -1}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {minimumFractionDigits: 21}), RangeError);

// The option maximumFractionDigits is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {maximumFractionDigits: 6}), [{locale: 'en', maximumFractionDigits: 6}]), true);
shouldThrow(() => Intl.NumberFormat('en', {minimumFractionDigits: 7, maximumFractionDigits: 6}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {maximumFractionDigits: -1}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {maximumFractionDigits: 21}), RangeError);

// The option minimumSignificantDigits is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {minimumSignificantDigits: 6}), [{locale: 'en', minimumFractionDigits: undefined, maximumFractionDigits: undefined, minimumSignificantDigits: 6, maximumSignificantDigits: 21}]), true);
shouldThrow(() => Intl.NumberFormat('en', {minimumSignificantDigits: 0}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {minimumSignificantDigits: 22}), RangeError);

// The option maximumSignificantDigits is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {maximumSignificantDigits: 6}), [{locale: 'en', minimumFractionDigits: undefined, maximumFractionDigits: undefined, minimumSignificantDigits: 1, maximumSignificantDigits: 6}]), true);
shouldThrow(() => Intl.NumberFormat('en', {minimumSignificantDigits: 7, maximumSignificantDigits: 6}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {maximumSignificantDigits: 0}), RangeError);
shouldThrow(() => Intl.NumberFormat('en', {maximumSignificantDigits: 22}), RangeError);

// The option useGrouping is processed correctly.
shouldBe(testNumberFormat(Intl.NumberFormat('en', {useGrouping: true}), [{locale: 'en', useGrouping: true}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {useGrouping: false}), [{locale: 'en', useGrouping: false}]), true);
shouldBe(testNumberFormat(Intl.NumberFormat('en', {useGrouping: 'false'}), [{locale: 'en', useGrouping: true}]), true);
shouldThrow(() => Intl.NumberFormat('en', { get useGrouping() { throw new Error(); } }), Error);

// 11.2 Properties of the Intl.NumberFormat Constructor

// length property (whose value is 0)
shouldBe(Intl.NumberFormat.length, 0);

// 11.2.1 Intl.NumberFormat.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat, 'prototype').configurable, false);

// 11.2.2 Intl.NumberFormat.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe(Intl.NumberFormat.supportedLocalesOf.length, 1);

// Returns SupportedLocales
shouldBe(Intl.NumberFormat.supportedLocalesOf() instanceof Array, true);
// Doesn't care about `this`.
shouldBe(JSON.stringify(Intl.NumberFormat.supportedLocalesOf.call(null, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.NumberFormat.supportedLocalesOf.call({}, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.NumberFormat.supportedLocalesOf.call(1, 'en')), '["en"]');
// Ignores non-object, non-string list.
shouldBe(JSON.stringify(Intl.NumberFormat.supportedLocalesOf(9)), '[]');
// Makes an array of tags.
shouldBe(JSON.stringify(Intl.NumberFormat.supportedLocalesOf('en')), '["en"]');
// Handles array-like objects with holes.
shouldBe(JSON.stringify(Intl.NumberFormat.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })), '["es","en","de"]');
// Deduplicates tags.
shouldBe(JSON.stringify(Intl.NumberFormat.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])), '["en","pt","es"]');
// Canonicalizes tags.
shouldBe(
    JSON.stringify(Intl.NumberFormat.supportedLocalesOf('En-laTn-us-variAnt-fOObar-1abc-U-kn-tRue-A-aa-aaa-x-RESERVED')),
    $vm.icuVersion() >= 67
        ? '["en-Latn-US-1abc-foobar-variant-a-aa-aaa-u-kn-x-reserved"]'
        : '["en-Latn-US-variant-foobar-1abc-a-aa-aaa-u-kn-true-x-reserved"]'
);
// Throws on problems with length, get, or toString.
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf(Object.create(null, { length: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf([ { toString() { throw new Error(); } } ]), Error);
// Throws on bad tags.
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('no-bok'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('x-some-thing'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf([ 5 ]), TypeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf(''), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('a'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('abcdefghij'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('#$'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('en-@-abc'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('en-u'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('en-u-kn-true-u-ko-true'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('en-x'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('en-*'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('en-'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('en--US'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('i-klingon'), RangeError); // grandfathered tag is not accepted by IsStructurallyValidLanguageTag
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('x-en-US-12345'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('x-12345-12345-en-US'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('x-en-US-12345-12345'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('x-en-u-foo'), RangeError);
shouldThrow(() => Intl.NumberFormat.supportedLocalesOf('x-en-u-foo-u-bar'), RangeError);

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
    'cmn-hans-cn-t-ca-u-ca-x-t-u', // singleton subtags can also be used as private use subtags
    'enochian-enochian', // language and variant subtags may be the same
    'de-gregory-u-ca-gregory', // variant and extension subtags may be the same
    'aa-a-foo-x-a-foo-bar', // variant subtags can also be used as private use subtags
];
for (var validLanguageTag of validLanguageTags)
    shouldNotThrow(() => Intl.NumberFormat.supportedLocalesOf(validLanguageTag));

// 11.4 Properties of the Intl.NumberFormat Prototype Object

// The Intl.NumberFormat prototype object is itself an ordinary object.
shouldBe(Object.getPrototypeOf(Intl.NumberFormat.prototype), Object.prototype);

// 11.4.1 Intl.NumberFormat.prototype.constructor
// The initial value of Intl.NumberFormat.prototype.constructor is the intrinsic object %NumberFormat%.
shouldBe(Intl.NumberFormat.prototype.constructor, Intl.NumberFormat);

// 11.4.2 Intl.NumberFormat.prototype [ @@toStringTag ]
// The initial value of the @@toStringTag property is the string value "Intl.NumberFormat".
shouldBe(Intl.NumberFormat.prototype[Symbol.toStringTag], 'Intl.NumberFormat');
shouldBe(Object.prototype.toString.call(Intl.NumberFormat.prototype), '[object Intl.NumberFormat]');
// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, Symbol.toStringTag).writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, Symbol.toStringTag).enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, Symbol.toStringTag).configurable, true);

// 11.4.3 Intl.NumberFormat.prototype.format

// This named accessor property returns a function that formats a number according to the effective locale and the formatting options of this NumberFormat object.
var defaultNFormat = Intl.NumberFormat('en');
shouldBe(defaultNFormat.format instanceof Function, true);

// The value of the [[Get]] attribute is a function
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').get instanceof Function, true);

// The value of the [[Set]] attribute is undefined.
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').set, undefined);

// Match Firefox where unspecifed.
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format').configurable, true);

// The value of F’s length property is 1.
shouldBe(defaultNFormat.format.length, 1);

// Throws on non-NumberFormat this.
shouldThrow(() => Intl.NumberFormat.prototype.format, TypeError);
shouldThrow(() => Object.defineProperty({}, 'format', Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'format')).format, TypeError);

// The format function is unique per instance.
shouldBe(new Intl.NumberFormat().format !== new Intl.NumberFormat().format, true);

// 11.3.4 Format Number Functions

// 1. Let nf be the this value.
// 2. Assert: Type(nf) is Object and nf has an [[initializedNumberFormat]] internal slot whose value is true.
// This should not be reachable, since format is bound to an initialized numberformat.

// 3. If value is not provided, let value be undefined.
// 4. Let x be ToNumber(value).
// 5. ReturnIfAbrupt(x).
shouldThrow(() => defaultNFormat.format({ valueOf() { throw new Error(); } }), Error);

// Format is bound, so calling with alternate this has no effect.
shouldBe(defaultNFormat.format.call(null, 1.2), Intl.NumberFormat().format(1.2));
shouldBe(defaultNFormat.format.call(Intl.DateTimeFormat('ar'), 1.2), Intl.NumberFormat().format(1.2));
shouldBe(defaultNFormat.format.call(5, 1.2), Intl.NumberFormat().format(1.2));

// Test various values.
shouldBe(Intl.NumberFormat('en').format(42), '42');
shouldBe(Intl.NumberFormat('en').format('42'), '42');
shouldBe(Intl.NumberFormat('en').format({ valueOf() { return 42; } }), '42');
shouldBe(Intl.NumberFormat('en').format('one'), 'NaN');
shouldBe(Intl.NumberFormat('en').format(NaN), 'NaN');
shouldBe(Intl.NumberFormat('en').format(Infinity), '∞');
shouldBe(Intl.NumberFormat('en').format(-Infinity), '-∞');
shouldBe(Intl.NumberFormat('en').format(0), '0');
shouldBe(Intl.NumberFormat('en').format(-0), '-0');
shouldBe(Intl.NumberFormat('en').format(Number.MIN_VALUE), '0');
shouldBe(Intl.NumberFormat('en', { maximumSignificantDigits: 15 }).format(Number.MAX_VALUE), '179,769,313,486,232,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000');

// Test locales.
shouldBe(Intl.NumberFormat('en').format(12345.678), '12,345.678');
shouldBe(Intl.NumberFormat('es').format(12345.678), '12.345,678');
shouldBe(Intl.NumberFormat('de').format(12345.678), '12.345,678');

// Test numbering systems.
shouldBe(Intl.NumberFormat('en-u-nu-latn').format(1234.567), '1,234.567');
shouldBe(Intl.NumberFormat('en-u-nu-fullwide').format(1234.567), '１,２３４.５６７');
shouldBe(Intl.NumberFormat('th-u-nu-thai').format(1234.567), '๑,๒๓๔.๕๖๗');
shouldBe(Intl.NumberFormat('zh-Hans-CN-u-nu-hanidec').format(1234.567), '一,二三四.五六七');

// Test the style option.
shouldBe(Intl.NumberFormat('en', {style: 'decimal'}).format(4.2), '4.2');
shouldBe(Intl.NumberFormat('en', {style: 'percent'}).format(4.2), '420%');
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'USD'}).format(4.2), '$4.20');

// Test the currency option.
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'USD'}).format(4), '$4.00');
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'USD'}).format(4.2), '$4.20');
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'USD'}).format(-4.2), '-$4.20');
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'USD'}).format(NaN).includes('NaN'), true);
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'USD'}).format(Infinity), '$∞');
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'JPY'}).format(4.2), '¥4');

// Test the currencyDisplay option.
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'xXx', currencyDisplay: 'code'}).format(4.2).includes('XXX'), true);
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'code'}).format(4).includes('USD'), true);
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'symbol'}).format(4), '$4.00');
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'USD', currencyDisplay: 'name'}).format(4), '4.00 US dollars');
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'JPY', currencyDisplay: 'code'}).format(-4.2).includes('JPY'), true);
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'JPY', currencyDisplay: 'symbol'}).format(-4.2), '-¥4');
shouldBe(Intl.NumberFormat('en', {style: 'currency', currency: 'JPY', currencyDisplay: 'name'}).format(-4.2), '-4 Japanese yen');
shouldBe(Intl.NumberFormat('fr', {style: 'currency', currency: 'USD', currencyDisplay: 'name'}).format(4), '4,00 dollars des États-Unis');
shouldBe(Intl.NumberFormat('fr', {style: 'currency', currency: 'JPY', currencyDisplay: 'name'}).format(4), '4 yens japonais');

// Test the minimumIntegerDigits option.
shouldBe(Intl.NumberFormat('en', {minimumIntegerDigits: 4}).format(12), '0,012');
shouldBe(Intl.NumberFormat('en', {minimumIntegerDigits: 4}).format(12345), '12,345');

// Test the minimumFractionDigits option.
shouldBe(Intl.NumberFormat('en', {minimumFractionDigits: 3}).format(1), '1.000');
shouldBe(Intl.NumberFormat('en', {minimumFractionDigits: 3}).format(1.2), '1.200');
shouldBe(Intl.NumberFormat('en', {minimumFractionDigits: 3}).format(1.2345), '1.235');

// Test the maximumFractionDigits option.
shouldBe(Intl.NumberFormat('en', {minimumFractionDigits: 3, maximumFractionDigits: 4}).format(1.2345), '1.2345');
shouldBe(Intl.NumberFormat('en', {minimumFractionDigits: 3, maximumFractionDigits: 4}).format(1.23454), '1.2345');
shouldBe(Intl.NumberFormat('en', {minimumFractionDigits: 3, maximumFractionDigits: 4}).format(1.23455), '1.2346');
shouldBe(Intl.NumberFormat('en', {maximumFractionDigits: 0}).format(0.5), '1');
shouldBe(Intl.NumberFormat('en', {maximumFractionDigits: 0}).format(0.4), '0');
shouldBe(Intl.NumberFormat('en', {maximumFractionDigits: 0}).format(-0.4), '-0');
shouldBe(Intl.NumberFormat('en', {maximumFractionDigits: 0}).format(-0.5), '-1');

// Test the minimumSignificantDigits option.
shouldBe(Intl.NumberFormat('en', {minimumSignificantDigits: 4}).format(0.12), '0.1200');
shouldBe(Intl.NumberFormat('en', {minimumSignificantDigits: 4}).format(1.2), '1.200');
shouldBe(Intl.NumberFormat('en', {minimumSignificantDigits: 4}).format(12), '12.00');
shouldBe(Intl.NumberFormat('en', {minimumSignificantDigits: 4}).format(123456), '123,456');

// Test the maximumSignificantDigits option.
shouldBe(Intl.NumberFormat('en', {maximumSignificantDigits: 4}).format(0.1), '0.1');
shouldBe(Intl.NumberFormat('en', {maximumSignificantDigits: 4}).format(0.1234567), '0.1235');
shouldBe(Intl.NumberFormat('en', {maximumSignificantDigits: 4}).format(1234567), '1,235,000');

// Test the useGrouping option.
shouldBe(Intl.NumberFormat('en', {useGrouping: true}).format(1234567.123), '1,234,567.123');
shouldBe(Intl.NumberFormat('es', {useGrouping: true}).format(1234567.123), '1.234.567,123');
shouldBe(Intl.NumberFormat('de', {useGrouping: true}).format(1234567.123), '1.234.567,123');
shouldBe(Intl.NumberFormat('en', {useGrouping: false}).format(1234567.123), '1234567.123');
shouldBe(Intl.NumberFormat('es', {useGrouping: false}).format(1234567.123), '1234567,123');
shouldBe(Intl.NumberFormat('de', {useGrouping: false}).format(1234567.123), '1234567,123');

// 11.3.5 Intl.NumberFormat.prototype.resolvedOptions ()

shouldBe(Intl.NumberFormat.prototype.resolvedOptions.length, 0);

// Returns a new object whose properties and attributes are set as if constructed by an object literal.
shouldBe(defaultNFormat.resolvedOptions() instanceof Object, true);

// Returns a new object each time.
shouldBe(defaultNFormat.resolvedOptions() !== defaultNFormat.resolvedOptions(), true);

// Throws on non-NumberFormat this.
shouldThrow(() => Intl.NumberFormat.prototype.resolvedOptions(), TypeError);
shouldThrow(() => Intl.NumberFormat.prototype.resolvedOptions.call(5), TypeError);

// Returns the default options.
{
    let options = defaultNFormat.resolvedOptions();
    delete options.locale; 
    shouldBe(JSON.stringify(options), '{"numberingSystem":"latn","style":"decimal","minimumIntegerDigits":1,"minimumFractionDigits":0,"maximumFractionDigits":3,"useGrouping":true,"notation":"standard","signDisplay":"auto"}');
}

// Legacy compatibility with ECMA-402 1.0
{
    let legacy = Object.create(Intl.NumberFormat.prototype);
    let incompat = {};
    shouldBe(Intl.NumberFormat.apply(legacy), legacy);
    shouldBe(Intl.NumberFormat.call(legacy, 'en-u-nu-arab').format(1.2345), '١٫٢٣٥');
    shouldBe(Intl.NumberFormat.apply(incompat) !== incompat, true);
}

// BigInt tests
shouldBe(Intl.NumberFormat().format(0n), '0');
shouldBe(Intl.NumberFormat().format(BigInt(1)), '1');
shouldBe(Intl.NumberFormat('ar').format(123456789n), '١٢٣٬٤٥٦٬٧٨٩');
shouldBe(Intl.NumberFormat('zh-Hans-CN-u-nu-hanidec').format(123456789n), '一二三,四五六,七八九');
shouldBe(Intl.NumberFormat('en', { maximumSignificantDigits: 3 }).format(123456n), '123,000');
