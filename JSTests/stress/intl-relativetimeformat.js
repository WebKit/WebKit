function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const icuVersion = $vm.icuVersion();
function shouldBeForICUVersion(minimumVersion, actual, expected) {
    if (icuVersion < minimumVersion)
        return;

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

shouldBe(Intl.RelativeTimeFormat instanceof Function, true);
shouldBe(Intl.RelativeTimeFormat.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(Intl.RelativeTimeFormat, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.RelativeTimeFormat, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.RelativeTimeFormat, 'prototype').configurable, false);

shouldThrow(() => Intl.RelativeTimeFormat(), TypeError);
shouldThrow(() => Intl.RelativeTimeFormat.call({}), TypeError);

shouldThrow(() => new Intl.RelativeTimeFormat('$'), RangeError);
shouldThrow(() => new Intl.RelativeTimeFormat('en', null), TypeError);

shouldBe(new Intl.RelativeTimeFormat() instanceof Intl.RelativeTimeFormat, true);

{
    class DerivedRelativeTimeFormat extends Intl.RelativeTimeFormat {}

    const drtf = new DerivedRelativeTimeFormat();
    shouldBe(drtf instanceof DerivedRelativeTimeFormat, true);
    shouldBe(drtf instanceof Intl.RelativeTimeFormat, true);
    shouldBe(drtf.format, Intl.RelativeTimeFormat.prototype.format);
    shouldBe(drtf.formatToParts, Intl.RelativeTimeFormat.prototype.formatToParts);
    shouldBe(Object.getPrototypeOf(drtf), DerivedRelativeTimeFormat.prototype);
    shouldBe(Object.getPrototypeOf(DerivedRelativeTimeFormat.prototype), Intl.RelativeTimeFormat.prototype);
}

shouldBe(Intl.RelativeTimeFormat.supportedLocalesOf.length, 1);
shouldBe(Intl.RelativeTimeFormat.supportedLocalesOf() instanceof Array, true);
shouldBe(JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf.call(null, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf.call({}, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf.call(1, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf(9)), '[]');
shouldBe(JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf('en')), '["en"]');
shouldBe(JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })), '["es","en","de"]');
shouldBe(JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf(['en', 'pt', 'en', 'es'])), '["en","pt","es"]');
shouldBe(
    JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf('En-laTn-us-variAnt-fOObar-1abc-U-kn-tRue-A-aa-aaa-x-RESERVED')),
    $vm.icuVersion() >= 67
        ? '["en-Latn-US-1abc-foobar-variant-a-aa-aaa-u-kn-x-reserved"]'
        : '["en-Latn-US-variant-foobar-1abc-a-aa-aaa-u-kn-true-x-reserved"]'
);
shouldBe(JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf('no-bok')), '["nb"]');
shouldBe(JSON.stringify(Intl.RelativeTimeFormat.supportedLocalesOf('x-some-thing')), '[]');

shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf(Object.create(null, { length: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf([{ toString() { throw new Error(); } }]), Error);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf([5]), TypeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf(''), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('a'), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('abcdefghij'), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('#$'), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('en-@-abc'), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('en-u'), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('en-u-kn-true-u-ko-true'), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('en-x'), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('en-*'), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('en-'), RangeError);
shouldThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf('en--US'), RangeError);

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
for (let validLanguageTag of validLanguageTags)
    shouldNotThrow(() => Intl.RelativeTimeFormat.supportedLocalesOf(validLanguageTag));

shouldBe(Object.getPrototypeOf(Intl.RelativeTimeFormat.prototype), Object.prototype);

shouldBe(Intl.RelativeTimeFormat.prototype.constructor, Intl.RelativeTimeFormat);

shouldBe(Intl.RelativeTimeFormat.prototype[Symbol.toStringTag], 'Intl.RelativeTimeFormat');
shouldBe(Object.prototype.toString.call(Intl.RelativeTimeFormat.prototype), '[object Intl.RelativeTimeFormat]');
shouldBe(Object.getOwnPropertyDescriptor(Intl.RelativeTimeFormat.prototype, Symbol.toStringTag).writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.RelativeTimeFormat.prototype, Symbol.toStringTag).enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.RelativeTimeFormat.prototype, Symbol.toStringTag).configurable, true);

shouldThrow(() => new Intl.RelativeTimeFormat('en', { localeMatcher: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.RelativeTimeFormat('en', { localeMatcher:'bad' }), RangeError);
shouldNotThrow(() => new Intl.RelativeTimeFormat('en', { localeMatcher:'lookup' }));
shouldNotThrow(() => new Intl.RelativeTimeFormat('en', { localeMatcher:'best fit' }));

const defaultRTF = new Intl.RelativeTimeFormat('en');

shouldBe(Intl.RelativeTimeFormat.prototype.resolvedOptions.length, 0);
shouldThrow(() => Intl.RelativeTimeFormat.prototype.resolvedOptions.call(5), TypeError);
shouldThrow(() => Intl.RelativeTimeFormat.prototype.resolvedOptions.call({}), TypeError);
shouldBe(defaultRTF.resolvedOptions() instanceof Object, true);
shouldBe(defaultRTF.resolvedOptions() !== defaultRTF.resolvedOptions(), true);
shouldBe(defaultRTF.resolvedOptions().locale, 'en');
shouldBe(defaultRTF.resolvedOptions().style, 'long');
shouldBe(defaultRTF.resolvedOptions().numeric, 'always');
shouldBe(defaultRTF.resolvedOptions().numberingSystem, 'latn');

shouldBe(new Intl.RelativeTimeFormat('en-u-nu-hanidec').resolvedOptions().locale, 'en-u-nu-hanidec');
shouldBe(new Intl.RelativeTimeFormat('en-u-nu-hanidec', { numberingSystem: 'gujr' }).resolvedOptions().locale, 'en');
shouldBe(new Intl.RelativeTimeFormat('en', { numberingSystem: 'hanidec' }).resolvedOptions().locale, 'en');
shouldBe(new Intl.RelativeTimeFormat('en-u-ca-chinese').resolvedOptions().locale, 'en');

shouldThrow(() => new Intl.RelativeTimeFormat('en', { style: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.RelativeTimeFormat('en', { style: 'bad' }), RangeError);
shouldBe(new Intl.RelativeTimeFormat('en', { style: 'long' }).resolvedOptions().style, 'long');
shouldBe(new Intl.RelativeTimeFormat('en', { style: 'short' }).resolvedOptions().style, 'short');
shouldBe(new Intl.RelativeTimeFormat('en', { style: 'narrow' }).resolvedOptions().style, 'narrow');

shouldThrow(() => new Intl.RelativeTimeFormat('en', { numeric: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => new Intl.RelativeTimeFormat('en', { numeric: 'bad' }), RangeError);
shouldBe(new Intl.RelativeTimeFormat('en', { numeric: 'always' }).resolvedOptions().numeric, 'always');
shouldBe(new Intl.RelativeTimeFormat('en', { numeric: 'auto' }).resolvedOptions().numeric, 'auto');

const numberingSystems = [
    'arab', 'arabext', 'bali', 'beng', 'deva', 'fullwide', 'gujr', 'guru',
    'hanidec', 'khmr', 'knda', 'laoo', 'latn', 'limb', 'mlym', 'mong', 'mymr',
    'orya', 'tamldec', 'telu', 'thai', 'tibt'
]
for (let numberingSystem of numberingSystems) {
    shouldBe(new Intl.RelativeTimeFormat('en', { numberingSystem }).resolvedOptions().numberingSystem, numberingSystem);
    shouldBe(new Intl.RelativeTimeFormat(`en-u-nu-${numberingSystem}`).resolvedOptions().numberingSystem, numberingSystem);
}

shouldBe(Intl.RelativeTimeFormat.prototype.format.length, 2);
shouldThrow(() => Intl.RelativeTimeFormat.prototype.format.call({}, 3, 'days'), TypeError);
shouldThrow(() => defaultRTF.format(Symbol(), 'days'), TypeError);
shouldThrow(() => defaultRTF.format(3, Symbol()), TypeError);
shouldThrow(() => defaultRTF.format(Infinity, 'days'), RangeError);
shouldThrow(() => defaultRTF.format(3, 'centuries'), RangeError);

const units = ['second', 'minute', 'hour', 'day', 'week', 'month', 'year'];
if (icuVersion >= 63)
    units.push('quarter');

for (let unit of units) {
    shouldBe(defaultRTF.format(10, unit), defaultRTF.format(10, `${unit}s`));

    shouldBe(defaultRTF.format(10000.5, unit), `in 10,000.5 ${unit}s`);
    shouldBe(defaultRTF.format(10, unit), `in 10 ${unit}s`);
    shouldBe(defaultRTF.format(0, unit), `in 0 ${unit}s`);
    shouldBe(defaultRTF.format(-10, unit), `10 ${unit}s ago`);
    shouldBe(defaultRTF.format(-10000.5, unit), `10,000.5 ${unit}s ago`);

    shouldBeForICUVersion(64, defaultRTF.format(1, unit), `in 1 ${unit}`);
    shouldBeForICUVersion(63, defaultRTF.format(-0, unit), `0 ${unit}s ago`);
    shouldBeForICUVersion(64, defaultRTF.format(-1, unit), `1 ${unit} ago`);
}

shouldBe(new Intl.RelativeTimeFormat('en', { style: 'short' }).format(10, 'second'), 'in 10 sec.');
shouldBe(new Intl.RelativeTimeFormat('ru', { style: 'short' }).format(10, 'second'), 'через 10 сек.');
shouldBe(new Intl.RelativeTimeFormat('en', { style: 'narrow' }).format(10, 'second'), 'in 10 sec.');
shouldBe(new Intl.RelativeTimeFormat('ru', { style: 'narrow' }).format(10, 'second'), '+10 с');

shouldBe(new Intl.RelativeTimeFormat('en', { numeric: 'auto' }).format(0, 'second'), 'now');
shouldBe(new Intl.RelativeTimeFormat('ja', { numeric: 'auto' }).format(0, 'second'), '今');
shouldBe(new Intl.RelativeTimeFormat('en', { numeric: 'auto' }).format(0, 'day'), 'today');
shouldBe(new Intl.RelativeTimeFormat('ja', { numeric: 'auto' }).format(0, 'day'), '今日');
shouldBe(new Intl.RelativeTimeFormat('en', { numeric: 'auto' }).format(0, 'year'), 'this year');
shouldBe(new Intl.RelativeTimeFormat('ja', { numeric: 'auto' }).format(0, 'year'), '今年');

shouldBe(new Intl.RelativeTimeFormat('en', { numberingSystem: 'thai' }).format(-10, 'hour'), '๑๐ hours ago');
shouldBe(new Intl.RelativeTimeFormat('en-u-nu-thai').format(-10, 'hour'), '๑๐ hours ago');
shouldBe(new Intl.RelativeTimeFormat('ko', { numberingSystem: 'hanidec' }).format(-10, 'hour'), '一〇시간 전');
shouldBe(new Intl.RelativeTimeFormat('ko-u-nu-hanidec').format(-10, 'hour'), '一〇시간 전');

shouldBe(Intl.RelativeTimeFormat.prototype.formatToParts.length, 2);
shouldThrow(() => Intl.RelativeTimeFormat.prototype.formatToParts.call({}, 3, 'days'), TypeError);
shouldThrow(() => defaultRTF.formatToParts(Symbol(), 'days'), TypeError);
shouldThrow(() => defaultRTF.formatToParts(3, Symbol()), TypeError);
shouldThrow(() => defaultRTF.formatToParts(Infinity, 'days'), RangeError);
shouldThrow(() => defaultRTF.formatToParts(3, 'centuries'), RangeError);

for (let unit of units) {
    shouldBe(JSON.stringify(defaultRTF.formatToParts(10, unit)), JSON.stringify(defaultRTF.formatToParts(10, `${unit}s`)));

    const concatenateValues = (parts) => parts.map(part => part.value).join('');

    shouldBe(concatenateValues(defaultRTF.formatToParts(10000.5, unit)), `in 10,000.5 ${unit}s`);
    shouldBe(concatenateValues(defaultRTF.formatToParts(10, unit)), `in 10 ${unit}s`);
    shouldBe(concatenateValues(defaultRTF.formatToParts(0, unit)), `in 0 ${unit}s`);
    shouldBe(concatenateValues(defaultRTF.formatToParts(-10, unit)), `10 ${unit}s ago`);
    shouldBe(concatenateValues(defaultRTF.formatToParts(-10000.5, unit)), `10,000.5 ${unit}s ago`);

    shouldBeForICUVersion(64, concatenateValues(defaultRTF.formatToParts(1, unit)), `in 1 ${unit}`);
    shouldBeForICUVersion(63, concatenateValues(defaultRTF.formatToParts(-0, unit)), `0 ${unit}s ago`);
    shouldBeForICUVersion(64, concatenateValues(defaultRTF.formatToParts(-1, unit)), `1 ${unit} ago`);
}

shouldBe(
    JSON.stringify(defaultRTF.formatToParts(10000.5, 'day')),
    JSON.stringify([
        { type: 'literal', value: 'in ' },
        { type: 'integer', value: '10', unit: 'day' },
        { type: 'group', value: ',', unit: 'day' },
        { type: 'integer', value: '000', unit: 'day' },
        { type: 'decimal', value: '.', unit: 'day' },
        { type: 'fraction', value: '5', unit: 'day' },
        { type: 'literal', value: ' days' }
    ])
);

shouldBe(
    JSON.stringify(new Intl.RelativeTimeFormat('sw').formatToParts(10, 'year')),
    JSON.stringify([
        { type: 'literal', value: 'baada ya miaka ' },
        { type: 'integer', value: '10', unit: 'year' },
    ])
);

shouldBe(
    JSON.stringify(new Intl.RelativeTimeFormat('ru', { style: 'narrow' }).formatToParts(10, 'second')),
    JSON.stringify([
        { type: 'literal', value: '+' },
        { type: 'integer', value: '10', unit: 'second' },
        { type: 'literal', value: ' с' }
    ])
);

shouldBe(
    JSON.stringify(new Intl.RelativeTimeFormat('ja', { numeric: 'auto' }).formatToParts(0, 'week')),
    JSON.stringify([
        { type: 'literal', value: '今週' }
    ])
);

shouldBe(
    JSON.stringify(new Intl.RelativeTimeFormat('ko', { numberingSystem: 'hanidec' }).formatToParts(-10, 'hour')),
    JSON.stringify([
        { type: 'integer', value: '一〇', unit: 'hour' },
        { type: 'literal', value: '시간 전' }
    ])
);
