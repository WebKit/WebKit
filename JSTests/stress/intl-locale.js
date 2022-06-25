function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldBeOneOf(actual, expectedArray) {
    if (!expectedArray.some((value) => value === actual))
        throw new Error('bad value: ' + actual + ' expected values: ' + expectedArray);
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

shouldBe(Intl.Locale instanceof Function, true);
shouldBe(Intl.Locale.length, 1);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Locale, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Locale, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Locale, 'prototype').configurable, false);

shouldThrow(() => Intl.Locale(), TypeError);
shouldThrow(() => Intl.Locale.call({}), TypeError);

shouldThrow(() => new Intl.Locale(), TypeError);
shouldThrow(() => new Intl.Locale(5), TypeError);
shouldThrow(() => new Intl.Locale(''), RangeError);
shouldThrow(() => new Intl.Locale('a'), RangeError);
shouldThrow(() => new Intl.Locale('abcdefghij'), RangeError);
shouldThrow(() => new Intl.Locale('#$'), RangeError);
shouldThrow(() => new Intl.Locale('en-@-abc'), RangeError);
shouldThrow(() => new Intl.Locale('en-u'), RangeError);
shouldThrow(() => new Intl.Locale('en-u-kn-true-u-ko-true'), RangeError);
shouldThrow(() => new Intl.Locale('en-x'), RangeError);
shouldThrow(() => new Intl.Locale('en-*'), RangeError);
shouldThrow(() => new Intl.Locale('en-'), RangeError);
shouldThrow(() => new Intl.Locale('en--US'), RangeError);
shouldThrow(() => new Intl.Locale(['en', 'ja']), RangeError);
shouldThrow(() => new Intl.Locale({}), RangeError);
shouldThrow(() => new Intl.Locale({ toString() { throw new Error(); } }), Error);
shouldThrow(() => new Intl.Locale([]), RangeError);
shouldThrow(() => new Intl.Locale('i-klingon'), RangeError); // grandfathered tag is not accepted by IsStructurallyValidLanguageTag
shouldThrow(() => new Intl.Locale('x-en-US-12345'), RangeError);
shouldThrow(() => new Intl.Locale('x-12345-12345-en-US'), RangeError);
shouldThrow(() => new Intl.Locale('x-en-US-12345-12345'), RangeError);
shouldThrow(() => new Intl.Locale('x-en-u-foo'), RangeError);
shouldThrow(() => new Intl.Locale('x-en-u-foo-u-bar'), RangeError);
shouldNotThrow(() => new Intl.Locale({ toString() { return 'en'; } }));

shouldThrow(() => new Intl.Locale('en', null), TypeError);

shouldBe(new Intl.Locale('en') instanceof Intl.Locale, true);

{
    class DerivedLocale extends Intl.Locale {}

    const dl = new DerivedLocale('en');
    shouldBe(dl instanceof DerivedLocale, true);
    shouldBe(dl instanceof Intl.Locale, true);
    shouldBe(dl.maximize, Intl.Locale.prototype.maximize);
    shouldBe(Object.getPrototypeOf(dl), DerivedLocale.prototype);
    shouldBe(Object.getPrototypeOf(DerivedLocale.prototype), Intl.Locale.prototype);
}

const validLanguageTags = [
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
for (let validLanguageTag of validLanguageTags)
    shouldNotThrow(() => new Intl.Locale(validLanguageTag));

shouldBe(Object.getPrototypeOf(Intl.Locale.prototype), Object.prototype);

shouldBe(Intl.Locale.prototype.constructor, Intl.Locale);

shouldBe(Intl.Locale.prototype[Symbol.toStringTag], 'Intl.Locale');
shouldBe(Object.prototype.toString.call(Intl.Locale.prototype), '[object Intl.Locale]');
shouldBe(Object.getOwnPropertyDescriptor(Intl.Locale.prototype, Symbol.toStringTag).writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Locale.prototype, Symbol.toStringTag).enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.Locale.prototype, Symbol.toStringTag).configurable, true);

shouldBe(Intl.Locale.prototype.maximize.length, 0);
shouldBe(Intl.Locale.prototype.minimize.length, 0);
shouldBe(Intl.Locale.prototype.toString.length, 0);
shouldThrow(() => Intl.Locale.prototype.maximize.call({ toString() { return 'en'; } }), TypeError);
shouldThrow(() => Intl.Locale.prototype.minimize.call({ toString() { return 'en'; } }), TypeError);
shouldThrow(() => Intl.Locale.prototype.toString.call({ toString() { return 'en'; } }), TypeError);
shouldThrow(() => Intl.Locale.prototype.baseName, TypeError);
shouldThrow(() => Intl.Locale.prototype.calendar, TypeError);
shouldThrow(() => Intl.Locale.prototype.caseFirst, TypeError);
shouldThrow(() => Intl.Locale.prototype.collation, TypeError);
shouldThrow(() => Intl.Locale.prototype.hourCycle, TypeError);
shouldThrow(() => Intl.Locale.prototype.numeric, TypeError);
shouldThrow(() => Intl.Locale.prototype.numberingSystem, TypeError);
shouldThrow(() => Intl.Locale.prototype.language, TypeError);
shouldThrow(() => Intl.Locale.prototype.script, TypeError);
shouldThrow(() => Intl.Locale.prototype.region, TypeError);

shouldBe(new Intl.Locale('en').maximize().toString(), 'en-Latn-US');
shouldBe(new Intl.Locale('en-Latn-US').maximize().toString(), 'en-Latn-US');
shouldBe(new Intl.Locale('en-u-nu-thai-x-foo').maximize().toString(), 'en-Latn-US-u-nu-thai-x-foo');
shouldBe(new Intl.Locale('ja').maximize().toString(), 'ja-Jpan-JP');
shouldBe(new Intl.Locale('zh').maximize().toString(), 'zh-Hans-CN');
shouldBe(new Intl.Locale('zh-Hant').maximize().toString(), 'zh-Hant-TW');
shouldBe(new Intl.Locale('zh', { script: 'Hant' }).maximize().toString(), 'zh-Hant-TW');

shouldBe(new Intl.Locale('en').minimize().toString(), 'en');
shouldBe(new Intl.Locale('en-Latn-US').minimize().toString(), 'en');
shouldBe(new Intl.Locale('en-Latn-US-u-nu-thai-x-foo').minimize().toString(), 'en-u-nu-thai-x-foo');
shouldBe(new Intl.Locale('ja-Jpan-JP').minimize().toString(), 'ja');
shouldBe(new Intl.Locale('zh-Hans-CN').minimize().toString(), 'zh');
shouldBe(new Intl.Locale('zh-Hant-TW').minimize().toString(), 'zh-TW');
shouldBe(new Intl.Locale('zh', { script: 'Hant', region: 'TW' }).minimize().toString(), 'zh-TW');

{
    let locale = new Intl.Locale('en')
    shouldBe(typeof locale.maximize(), 'object');
    shouldBe(typeof locale.minimize(), 'object');
    shouldBe(Object.prototype.toString.call(locale.maximize()), '[object Intl.Locale]');
    shouldBe(Object.prototype.toString.call(locale.minimize()), '[object Intl.Locale]');
    shouldBe(locale.maximize().baseName, 'en-Latn-US');
    shouldBe(locale.minimize().baseName, 'en');
}

{
    let locale = new Intl.Locale('ja-Jpan-JP-u-ca-japanese-hc-h23-kf-lower-co-emoji-nu-jpan')
    shouldBe(locale.maximize().toString(), 'ja-Jpan-JP-u-ca-japanese-co-emoji-hc-h23-kf-lower-nu-jpan');
    shouldBe(locale.maximize().baseName, 'ja-Jpan-JP');
}

shouldBe(new Intl.Locale('en').toString(), 'en');
shouldBe(
    new Intl.Locale('En-laTn-us-variAnt-fOObar-1abc-U-kn-tRue-A-aa-aaa-x-RESERVED').toString(),
    $vm.icuVersion() >= 67
        ? 'en-Latn-US-1abc-foobar-variant-a-aa-aaa-u-kn-x-reserved'
        : 'en-Latn-US-variant-foobar-1abc-a-aa-aaa-u-kn-x-reserved'
);
shouldBeOneOf(new Intl.Locale('cel-gaulish', { script: 'Arab', numberingSystem: 'gujr' }).toString(), [ 'xtg-Arab-u-nu-gujr', 'xtg-Arab-u-nu-gujr-x-cel-gaulish', 'cel-Arab-gaulish-u-nu-gujr' ]);
shouldBe(new Intl.Locale('en-Latn-US-u-ca-gregory-co-phonebk-hc-h12-kf-upper-kn-false-nu-latn').toString(), 'en-Latn-US-u-ca-gregory-co-phonebk-hc-h12-kf-upper-kn-false-nu-latn');

const options = {
    calendar: 'buddhist',
    caseFirst: 'lower',
    collation: 'eor',
    hourCycle: 'h11',
    numeric: false,
    numberingSystem: 'thai',
    language: 'ja',
    script: 'Hant',
    region: 'KR'
};
const expected = 'ja-Hant-KR-u-ca-buddhist-co-eor-hc-h11-kf-lower-kn-false-nu-thai';
shouldBe(new Intl.Locale('en', options).toString(), expected);
shouldBe(new Intl.Locale('en-Latn-US-u-ca-gregory-co-phonebk-hc-h12-kf-upper-kn-nu-latn', options).toString(), expected);

shouldBe(new Intl.Locale('en').baseName, 'en');
shouldBe(new Intl.Locale('en-Latn').baseName, 'en-Latn');
shouldBe(new Intl.Locale('en-US').baseName, 'en-US');
shouldBe(new Intl.Locale('en-Latn-US').baseName, 'en-Latn-US');
shouldBe(new Intl.Locale('en-variant-u-kn').baseName, 'en-variant');
shouldBe(new Intl.Locale('en-Latn-variant-u-kn').baseName, 'en-Latn-variant');
shouldBe(new Intl.Locale('en-variant-u-kn', { script: 'Latn' }).baseName, 'en-Latn-variant');
shouldBe(new Intl.Locale('en-US-variant-u-kn').baseName, 'en-US-variant');
shouldBe(new Intl.Locale('en-variant-u-kn', { region: 'US' }).baseName, 'en-US-variant');
shouldBe(new Intl.Locale('en-Latn-US-variant-u-kn').baseName, 'en-Latn-US-variant');
shouldBe(new Intl.Locale('en-variant-u-kn', { script: 'Latn', region: 'US' }).baseName, 'en-Latn-US-variant');

shouldBe(new Intl.Locale('en').calendar, undefined);
shouldBe(new Intl.Locale('en-u-ca-japanese').calendar, 'japanese');
shouldBe(new Intl.Locale('en', { calendar: 'japanese' }).calendar, 'japanese');
shouldBe(new Intl.Locale('en-u-ca-japanese', { calendar: 'dangi' }).calendar, 'dangi');
shouldBe(new Intl.Locale('en-u-ca-islamicc').calendar, 'islamic-civil');
shouldBe(new Intl.Locale('en', { calendar: 'islamicc' }).calendar, 'islamic-civil');
shouldThrow(() => new Intl.Locale('en', { calendar: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.Locale('en', { calendar: 'ng' }), RangeError);

shouldBe(new Intl.Locale('en').collation, undefined);
shouldBe(new Intl.Locale('en-u-co-phonebk').collation, 'phonebk');
shouldBe(new Intl.Locale('en', { collation: 'phonebk' }).collation, 'phonebk');
shouldBe(new Intl.Locale('en-u-co-phonebk', { collation: 'eor' }).collation, 'eor');
shouldThrow(() => new Intl.Locale('en', { collation: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.Locale('en', { collation: 'ng' }), RangeError);

shouldBe(new Intl.Locale('en').caseFirst, undefined);
shouldBe(new Intl.Locale('en-u-kf-upper').caseFirst, 'upper');
shouldBe(new Intl.Locale('en', { caseFirst: 'upper' }).caseFirst, 'upper');
shouldBe(new Intl.Locale('en-u-kf-upper', { caseFirst: 'lower' }).caseFirst, 'lower');
shouldThrow(() => new Intl.Locale('en', { caseFirst: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.Locale('en', { caseFirst: 'bad' }), RangeError);

shouldBe(new Intl.Locale('en').numeric, false);
shouldBe(new Intl.Locale('en-u-kn').numeric, true);
shouldBe(new Intl.Locale('en-u-kn-true').numeric, true);
shouldBe(new Intl.Locale('en', { numeric: true }).numeric, true);
shouldBe(new Intl.Locale('en-u-kn', { numeric: false }).numeric, false);
shouldBe(new Intl.Locale('en-u-kn-true', { numeric: false }).numeric, false);
shouldThrow(() => new Intl.Locale('en', { get numeric() { throw new Error(); } }), Error);

shouldBe(new Intl.Locale('en').hourCycle, undefined);
shouldBe(new Intl.Locale('en-u-hc-h11').hourCycle, 'h11');
shouldBe(new Intl.Locale('en', { hourCycle: 'h11' }).hourCycle, 'h11');
shouldBe(new Intl.Locale('en-u-hc-h11', { hourCycle: 'h23' }).hourCycle, 'h23');
shouldThrow(() => new Intl.Locale('en', { hourCycle: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.Locale('en', { hourCycle: 'bad' }), RangeError);

shouldBe(new Intl.Locale('en').numberingSystem, undefined);
shouldBe(new Intl.Locale('en-u-nu-hanidec').numberingSystem, 'hanidec');
shouldBe(new Intl.Locale('en', { numberingSystem: 'hanidec' }).numberingSystem, 'hanidec');
shouldBe(new Intl.Locale('en-u-nu-hanidec', { numberingSystem: 'gujr' }).numberingSystem, 'gujr');
shouldThrow(() => new Intl.Locale('en', { numberingSystem: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.Locale('en', { numberingSystem: 'ng' }), RangeError);

shouldBe(new Intl.Locale('en-Latn-US').language, 'en');
shouldBe(new Intl.Locale('en', { language: 'ar' }).language, 'ar');
shouldBeOneOf(new Intl.Locale('cel-gaulish').language, [ 'xtg', 'cel' ]);
shouldThrow(() => new Intl.Locale('en', { language: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.Locale('en', { language: 'fail' }), RangeError);

shouldBe(new Intl.Locale('en-Latn-US').script, 'Latn');
shouldBe(new Intl.Locale('en-US').script, undefined);
shouldBe(new Intl.Locale('en-Latn', { script: 'Kore' }).script, 'Kore');
shouldThrow(() => new Intl.Locale('en', { script: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.Locale('en', { script: 'bad' }), RangeError);

shouldBe(new Intl.Locale('en-Latn-US').region, 'US');
shouldBe(new Intl.Locale('en-Latn').region, undefined);
shouldBe(new Intl.Locale('en-US', { region: 'KR' }).region, 'KR');
shouldThrow(() => new Intl.Locale('en', { region: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.Locale('en', { region: 'fail' }), RangeError);
