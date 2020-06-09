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

// 12.1 The Intl.DateTimeFormat Constructor

// The Intl.DateTimeFormat constructor is a standard built-in property of the Intl object.
shouldBe(Intl.DateTimeFormat instanceof Function, true);

// 12.1.2 Intl.DateTimeFormat([ locales [, options]])
shouldBe(Intl.DateTimeFormat() instanceof Intl.DateTimeFormat, true);
shouldBe(Intl.DateTimeFormat.call({}) instanceof Intl.DateTimeFormat, true);
shouldBe(new Intl.DateTimeFormat() instanceof Intl.DateTimeFormat, true);

// Subclassable
{
    class DerivedDateTimeFormat extends Intl.DateTimeFormat {}
    shouldBe((new DerivedDateTimeFormat) instanceof DerivedDateTimeFormat, true);
    shouldBe((new DerivedDateTimeFormat) instanceof Intl.DateTimeFormat, true);
    shouldBe(new DerivedDateTimeFormat('en').format(0).length > 0, true);
    shouldBe(Object.getPrototypeOf(new DerivedDateTimeFormat), DerivedDateTimeFormat.prototype);
    shouldBe(Object.getPrototypeOf(Object.getPrototypeOf(new DerivedDateTimeFormat)), Intl.DateTimeFormat.prototype);
}

// 12.2 Properties of the Intl.DateTimeFormat Constructor

// length property (whose value is 0)
shouldBe(Intl.DateTimeFormat.length, 0);

// 12.2.1 Intl.DateTimeFormat.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat, 'prototype').configurable, false);

// 12.2.2 Intl.DateTimeFormat.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe(Intl.DateTimeFormat.supportedLocalesOf.length, 1);

// Returns SupportedLocales
shouldBe(Intl.DateTimeFormat.supportedLocalesOf() instanceof Array, true);
// Doesn't care about `this`.
shouldBe(JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf.call(null, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf.call({}, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf.call(1, 'en')), '["en"]');
// Ignores non-object, non-string list.
shouldBe(JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf(9)), '[]');
// Makes an array of tags.
shouldBe(JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf('en')), '["en"]');
// Handles array-like objects with holes.
shouldBe(JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })), '["es","en","de"]');
// Deduplicates tags.
shouldBe(JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])), '["en","pt","es"]');
// Canonicalizes tags.
shouldBe(
    JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf('En-laTn-us-variAnt-fOObar-1abc-U-kn-tRue-A-aa-aaa-x-RESERVED')),
    $vm.icuVersion() >= 67
        ? '["en-Latn-US-1abc-foobar-variant-a-aa-aaa-u-kn-x-reserved"]'
        : '["en-Latn-US-variant-foobar-1abc-a-aa-aaa-u-kn-true-x-reserved"]'
);
// Replaces outdated tags.
shouldBe(JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf('no-bok')), '["nb"]');
// Doesn't throw, but ignores private tags.
shouldBe(JSON.stringify(Intl.DateTimeFormat.supportedLocalesOf('x-some-thing')), '[]');
// Throws on problems with length, get, or toString.
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf(Object.create(null, { length: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf([ { toString() { throw new Error(); } } ]), Error);
// Throws on bad tags.
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf([ 5 ]), TypeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf(''), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('a'), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('abcdefghij'), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('#$'), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('en-@-abc'), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('en-u'), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('en-u-kn-true-u-ko-true'), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('en-x'), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('en-*'), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('en-'), RangeError);
shouldThrow(() => Intl.DateTimeFormat.supportedLocalesOf('en--US'), RangeError);
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
    shouldNotThrow(() => Intl.DateTimeFormat.supportedLocalesOf(validLanguageTag));

// 12.4 Properties of the Intl.DateTimeFormat Prototype Object

// The Intl.DateTimeFormat prototype object is itself an ordinary object.
shouldBe(Object.getPrototypeOf(Intl.DateTimeFormat.prototype), Object.prototype);

// 12.4.1 Intl.DateTimeFormat.prototype.constructor
// The initial value of Intl.DateTimeFormat.prototype.constructor is the intrinsic object %DateTimeFormat%.
shouldBe(Intl.DateTimeFormat.prototype.constructor, Intl.DateTimeFormat);

// 12.4.2 Intl.DateTimeFormat.prototype [ @@toStringTag ]
// The initial value of the @@toStringTag property is the string value "Intl.DateTimeFormat".
shouldBe(Intl.DateTimeFormat.prototype[Symbol.toStringTag], 'Intl.DateTimeFormat');
shouldBe(Object.prototype.toString.call(Intl.DateTimeFormat.prototype), '[object Intl.DateTimeFormat]');
// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, Symbol.toStringTag).writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, Symbol.toStringTag).enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, Symbol.toStringTag).configurable, true);

// 12.4.3 Intl.DateTimeFormat.prototype.format

// This named accessor property returns a function that formats a date according to the effective locale and the formatting options of this DateTimeFormat object.
var defaultDTFormat = Intl.DateTimeFormat('en');
shouldBe(defaultDTFormat.format instanceof Function, true);

// The value of the [[Get]] attribute is a function
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').get instanceof Function, true);

// The value of the [[Set]] attribute is undefined.
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').set, undefined);

// Match Firefox where unspecifed.
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').configurable, true);

// The value of F’s length property is 1.
shouldBe(defaultDTFormat.format.length, 1);

// Throws on non-DateTimeFormat this.
shouldThrow(() => Intl.DateTimeFormat.prototype.format, TypeError);
shouldThrow(() => Object.defineProperty({}, 'format', Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format')).format, TypeError);

// The format function is unique per instance.
shouldBe(new Intl.DateTimeFormat().format !== new Intl.DateTimeFormat().format, true);

// 12.3.4 DateTime Format Functions

// 1. Let dtf be the this value.
// 2. Assert: Type(dtf) is Object and dtf has an [[initializedDateTimeFormat]] internal slot whose value is true.
// This should not be reachable, since format is bound to an initialized datetimeformat.

// 3. If date is not provided or is undefined, then
// a. Let x be %Date_now%().
// 4. Else
// a. Let x be ToNumber(date).
// b. ReturnIfAbrupt(x).
shouldThrow(() => defaultDTFormat.format({ valueOf() { throw new Error(); } }), Error);

// 12.3.4 FormatDateTime abstract operation

// 1. If x is not a finite Number, then throw a RangeError exception.
shouldThrow(() => defaultDTFormat.format(Infinity), RangeError);

// Format is bound, so calling with alternate "this" has no effect.
shouldBe(defaultDTFormat.format.call(null, 0), Intl.DateTimeFormat().format(0));
shouldBe(defaultDTFormat.format.call(Intl.DateTimeFormat('ar'), 0), Intl.DateTimeFormat().format(0));
shouldBe(defaultDTFormat.format.call(5, 0), Intl.DateTimeFormat().format(0));

shouldBe(typeof defaultDTFormat.format(), 'string');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'America/Denver' }).format(new Date(1451099872641)), '12/25/2015');

// 12.3.5 Intl.DateTimeFormat.prototype.resolvedOptions ()

shouldBe(Intl.DateTimeFormat.prototype.resolvedOptions.length, 0);

// Returns a new object whose properties and attributes are set as if constructed by an object literal.
shouldBe(defaultDTFormat.resolvedOptions() instanceof Object, true);

// Returns a new object each time.
shouldBe(defaultDTFormat.resolvedOptions() !== defaultDTFormat.resolvedOptions(), true);

// Throws on non-DateTimeFormat this.
shouldThrow(() => Intl.DateTimeFormat.prototype.resolvedOptions(), TypeError);
shouldThrow(() => Intl.DateTimeFormat.prototype.resolvedOptions.call(5), TypeError);

shouldThrow(() => Intl.DateTimeFormat('$'), RangeError);
shouldThrow(() => Intl.DateTimeFormat('en', null), TypeError);

shouldBe(new Intl.DateTimeFormat('en-u-nu-latn-hc-h12-ca-gregory-kf-upper').resolvedOptions().locale, 'en-u-ca-gregory-hc-h12-nu-latn');

// Defaults to month, day, year.
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().weekday, undefined);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().era, undefined);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().month, 'numeric');
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().day, 'numeric');
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().year, 'numeric');
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().hour, undefined);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().hourCycle, undefined);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().hour12, undefined);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().minute, undefined);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().second, undefined);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().timeZoneName, undefined);

// Locale-sensitive format().
shouldBe(Intl.DateTimeFormat('ar', { timeZone: 'America/Denver' }).format(1451099872641), '٢٥‏/١٢‏/٢٠١٥');
shouldBe(Intl.DateTimeFormat('de', { timeZone: 'America/Denver' }).format(1451099872641), '25.12.2015');
shouldBe(Intl.DateTimeFormat('ja', { timeZone: 'America/Denver' }).format(1451099872641), '2015/12/25');
shouldBe(Intl.DateTimeFormat('pt', { timeZone: 'America/Denver' }).format(1451099872641), '25/12/2015');

shouldThrow(() => Intl.DateTimeFormat('en', { localeMatcher: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { localeMatcher: 'bad' }), RangeError);
shouldNotThrow(() => Intl.DateTimeFormat('en', { localeMatcher: 'lookup' }));
shouldNotThrow(() => Intl.DateTimeFormat('en', { localeMatcher: 'best fit' }));

shouldThrow(() => Intl.DateTimeFormat('en', { formatMatcher: { toString() { throw new Error(); } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { formatMatcher: 'bad' }), RangeError);
shouldNotThrow(() => Intl.DateTimeFormat('en', { formatMatcher: 'basic' }));
shouldNotThrow(() => Intl.DateTimeFormat('en', { formatMatcher: 'best fit' }));

shouldThrow(() => Intl.DateTimeFormat('en', { timeZone: 'nowhere/bogus' }), RangeError);
shouldThrow(() => Intl.DateTimeFormat('en', { timeZone: { toString() { throw new Error(); } } }), Error);

// Time zone is case insensitive.
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'america/denver' }).resolvedOptions().timeZone, 'America/Denver');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'AMERICA/LOS_ANGELES' }).resolvedOptions().timeZone, 'America/Los_Angeles');

// Default time zone is a valid canonical time zone.
shouldBe(Intl.DateTimeFormat('en', { timeZone: Intl.DateTimeFormat().resolvedOptions().timeZone }).resolvedOptions().timeZone, Intl.DateTimeFormat().resolvedOptions().timeZone);

// Time zone is canonicalized for obsolete links in IANA tz backward file.
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Australia/ACT' }).resolvedOptions().timeZone, 'Australia/Sydney');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Australia/North' }).resolvedOptions().timeZone, 'Australia/Darwin');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Australia/South' }).resolvedOptions().timeZone, 'Australia/Adelaide');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Australia/West' }).resolvedOptions().timeZone, 'Australia/Perth');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Brazil/East' }).resolvedOptions().timeZone, 'America/Sao_Paulo');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Brazil/West' }).resolvedOptions().timeZone, 'America/Manaus');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Canada/Atlantic' }).resolvedOptions().timeZone, 'America/Halifax');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Canada/Central' }).resolvedOptions().timeZone, 'America/Winnipeg');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Canada/Eastern' }).resolvedOptions().timeZone, 'America/Toronto');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Canada/Mountain' }).resolvedOptions().timeZone, 'America/Edmonton');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Canada/Pacific' }).resolvedOptions().timeZone, 'America/Vancouver');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'GB' }).resolvedOptions().timeZone, 'Europe/London');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'GMT+0' }).resolvedOptions().timeZone, 'UTC');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'GMT-0' }).resolvedOptions().timeZone, 'UTC');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'GMT0' }).resolvedOptions().timeZone, 'UTC');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Greenwich' }).resolvedOptions().timeZone, 'UTC');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'UCT' }).resolvedOptions().timeZone, 'UTC');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'US/Central' }).resolvedOptions().timeZone, 'America/Chicago');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'US/Eastern' }).resolvedOptions().timeZone, 'America/New_York');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'US/Michigan' }).resolvedOptions().timeZone, 'America/Detroit');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'US/Mountain' }).resolvedOptions().timeZone, 'America/Denver');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'US/Pacific' }).resolvedOptions().timeZone, 'America/Los_Angeles');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'UTC' }).resolvedOptions().timeZone, 'UTC');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Universal' }).resolvedOptions().timeZone, 'UTC');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Zulu' }).resolvedOptions().timeZone, 'UTC');

// Timezone-sensitive format().
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '12/25/2015');
shouldBe(Intl.DateTimeFormat('en', { timeZone: 'Pacific/Auckland' }).format(1451099872641), '12/26/2015');

// Gets default calendar and numberingSystem from locale.
shouldBe(Intl.DateTimeFormat('ar-sa').resolvedOptions().locale, 'ar-SA');
shouldBe(Intl.DateTimeFormat('fa-IR').resolvedOptions().calendar, 'persian');
shouldBe(Intl.DateTimeFormat('ar').resolvedOptions().numberingSystem, 'arab');

shouldBe(Intl.DateTimeFormat('en', { calendar: 'dangi' }).resolvedOptions().calendar, 'dangi');
shouldBe(Intl.DateTimeFormat('en-u-ca-bogus').resolvedOptions().locale, 'en');
shouldBe(Intl.DateTimeFormat('en-u-ca-bogus').resolvedOptions().calendar, 'gregory');
shouldBe(Intl.DateTimeFormat('en-u-ca-buddhist').resolvedOptions().locale, 'en-u-ca-buddhist');
shouldBe(Intl.DateTimeFormat('en-u-ca-buddhist').resolvedOptions().calendar, 'buddhist');
shouldBe(Intl.DateTimeFormat('en-u-ca-chinese').resolvedOptions().calendar, 'chinese');
shouldBe(Intl.DateTimeFormat('en-u-ca-coptic').resolvedOptions().calendar, 'coptic');
shouldBe(Intl.DateTimeFormat('en-u-ca-dangi').resolvedOptions().calendar, 'dangi');
shouldBeForICUVersion(62, Intl.DateTimeFormat('en-u-ca-ethioaa').resolvedOptions().calendar, 'ethiopic-amete-alem');
shouldBe(Intl.DateTimeFormat('en-u-ca-ethiopic').resolvedOptions().calendar, 'ethiopic');
shouldBe(Intl.DateTimeFormat('ar-SA-u-ca-gregory').resolvedOptions().calendar, 'gregory');
shouldBe(Intl.DateTimeFormat('en-u-ca-hebrew').resolvedOptions().calendar, 'hebrew');
shouldBe(Intl.DateTimeFormat('en-u-ca-indian').resolvedOptions().calendar, 'indian');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic').resolvedOptions().calendar, 'islamic');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamicc').resolvedOptions().calendar, 'islamic-civil');
shouldBe(Intl.DateTimeFormat('en-u-ca-ISO8601').resolvedOptions().calendar, 'iso8601');
shouldBe(Intl.DateTimeFormat('en-u-ca-japanese').resolvedOptions().calendar, 'japanese');
shouldBe(Intl.DateTimeFormat('en-u-ca-persian').resolvedOptions().calendar, 'persian');
shouldBe(Intl.DateTimeFormat('en-u-ca-roc').resolvedOptions().calendar, 'roc');
shouldBeForICUVersion(62, Intl.DateTimeFormat('en-u-ca-ethiopic-amete-alem').resolvedOptions().calendar, 'ethiopic-amete-alem');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic-umalqura').resolvedOptions().calendar, 'islamic-umalqura');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic-tbla').resolvedOptions().calendar, 'islamic-tbla');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic-civil').resolvedOptions().calendar, 'islamic-civil');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic-rgsa').resolvedOptions().calendar, 'islamic-rgsa');

// Calendar-sensitive format().
shouldBe(Intl.DateTimeFormat('en-u-ca-buddhist', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '12/25/2558 BE');
shouldBe(Intl.DateTimeFormat('en-u-ca-chinese', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '11/15/2015');
shouldBe(Intl.DateTimeFormat('en-u-ca-coptic', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '4/15/1732 ERA1');
shouldBe(Intl.DateTimeFormat('en-u-ca-dangi', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '11/15/2015');
shouldBeForICUVersion(62, Intl.DateTimeFormat('en-u-ca-ethioaa', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '4/15/7508 ERA0');
shouldBe(Intl.DateTimeFormat('en-u-ca-ethiopic', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '4/15/2008 ERA1');
shouldBe(Intl.DateTimeFormat('en-u-ca-gregory', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '12/25/2015');
shouldBe(Intl.DateTimeFormat('en-u-ca-hebrew', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '13 Tevet 5776');
shouldBe(Intl.DateTimeFormat('en-u-ca-indian', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '10/4/1937 Saka');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '3/14/1437 AH');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamicc', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '3/13/1437 AH');
shouldBe(Intl.DateTimeFormat('en-u-ca-ISO8601', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '12/25/2015');
shouldBe(Intl.DateTimeFormat('en-u-ca-japanese', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '12/25/27 H');
shouldBe(Intl.DateTimeFormat('en-u-ca-persian', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '10/4/1394 AP');
shouldBe(Intl.DateTimeFormat('en-u-ca-roc', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '12/25/104 Minguo');
shouldBeForICUVersion(62, Intl.DateTimeFormat('en-u-ca-ethiopic-amete-alem', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '4/15/7508 ERA0');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic-umalqura', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '3/14/1437 AH');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic-tbla', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '3/14/1437 AH');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic-civil', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '3/13/1437 AH');
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic-rgsa', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '3/14/1437 AH');

shouldBe(Intl.DateTimeFormat('en', { numberingSystem: 'gujr' }).resolvedOptions().numberingSystem, 'gujr');
shouldBe(Intl.DateTimeFormat('en-u-nu-bogus').resolvedOptions().locale, 'en');
shouldBe(Intl.DateTimeFormat('en-u-nu-bogus').resolvedOptions().numberingSystem, 'latn');
shouldBe(Intl.DateTimeFormat('en-u-nu-latn').resolvedOptions().numberingSystem, 'latn');
shouldBe(Intl.DateTimeFormat('en-u-nu-arab').resolvedOptions().locale, 'en-u-nu-arab');

let numberingSystems = [
    'arab', 'arabext', 'bali', 'beng', 'deva', 'fullwide', 'gujr', 'guru',
    'hanidec', 'khmr', 'knda', 'laoo', 'latn', 'limb', 'mlym', 'mong', 'mymr',
    'orya', 'tamldec', 'telu', 'thai', 'tibt'
]
for (let numberingSystem of numberingSystems)
    shouldBe(Intl.DateTimeFormat(`en-u-nu-${numberingSystem}`).resolvedOptions().numberingSystem, numberingSystem);

// Numbering system sensitive format().
shouldBe(Intl.DateTimeFormat('en-u-nu-arab', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '١٢/٢٥/٢٠١٥');
shouldBe(Intl.DateTimeFormat('en-u-nu-arabext', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '۱۲/۲۵/۲۰۱۵');
shouldBe(Intl.DateTimeFormat('en-u-nu-bali', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '᭑᭒/᭒᭕/᭒᭐᭑᭕');
shouldBe(Intl.DateTimeFormat('en-u-nu-beng', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '১২/২৫/২০১৫');
shouldBe(Intl.DateTimeFormat('en-u-nu-deva', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '१२/२५/२०१५');
shouldBe(Intl.DateTimeFormat('en-u-nu-fullwide', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '１２/２５/２０１５');
shouldBe(Intl.DateTimeFormat('en-u-nu-gujr', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '૧૨/૨૫/૨૦૧૫');
shouldBe(Intl.DateTimeFormat('en-u-nu-guru', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '੧੨/੨੫/੨੦੧੫');
shouldBe(Intl.DateTimeFormat('en-u-nu-hanidec', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '一二/二五/二〇一五');
shouldBe(Intl.DateTimeFormat('en-u-nu-khmr', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '១២/២៥/២០១៥');
shouldBe(Intl.DateTimeFormat('en-u-nu-knda', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '೧೨/೨೫/೨೦೧೫');
shouldBe(Intl.DateTimeFormat('en-u-nu-laoo', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '໑໒/໒໕/໒໐໑໕');
shouldBe(Intl.DateTimeFormat('en-u-nu-latn', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '12/25/2015');
shouldBe(Intl.DateTimeFormat('en-u-nu-limb', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '᥇᥈/᥈᥋/᥈᥆᥇᥋');
shouldBe(Intl.DateTimeFormat('en-u-nu-mlym', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '൧൨/൨൫/൨൦൧൫');
shouldBe(Intl.DateTimeFormat('en-u-nu-mong', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '᠑᠒/᠒᠕/᠒᠐᠑᠕');
shouldBe(Intl.DateTimeFormat('en-u-nu-mymr', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '၁၂/၂၅/၂၀၁၅');
shouldBe(Intl.DateTimeFormat('en-u-nu-orya', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '୧୨/୨୫/୨୦୧୫');
shouldBe(Intl.DateTimeFormat('en-u-nu-tamldec', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '௧௨/௨௫/௨௦௧௫');
shouldBe(Intl.DateTimeFormat('en-u-nu-telu', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '౧౨/౨౫/౨౦౧౫');
shouldBe(Intl.DateTimeFormat('en-u-nu-thai', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '๑๒/๒๕/๒๐๑๕');
shouldBe(Intl.DateTimeFormat('en-u-nu-tibt', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '༡༢/༢༥/༢༠༡༥');

// Hour cycle sensitive format().
shouldBe(Intl.DateTimeFormat('en-u-hc-h11').resolvedOptions().locale, 'en-u-hc-h11')
shouldBe(Intl.DateTimeFormat('en-u-hc-h11', { hour: 'numeric', hourCycle: 'h12' }).resolvedOptions().locale, 'en')
shouldBe(Intl.DateTimeFormat('en-u-hc-h11', { hour: 'numeric', hourCycle: 'h12' }).resolvedOptions().hourCycle, 'h12')
shouldBe(Intl.DateTimeFormat('en-u-hc-h11', { hour: 'numeric', hourCycle: 'h11', hour12: true }).resolvedOptions().locale, 'en')
shouldBe(Intl.DateTimeFormat('en-u-hc-h11', { hour: 'numeric', hourCycle: 'h11', hour12: true }).resolvedOptions().hourCycle, 'h12')
shouldBe(Intl.DateTimeFormat('en-u-hc-h11', { hour: 'numeric', hourCycle: 'h11', hour12: false }).resolvedOptions().hourCycle, 'h23')
shouldBe(Intl.DateTimeFormat('en-u-hc-h11', { hour: 'numeric' }).resolvedOptions().hourCycle, 'h11')
shouldBe(Intl.DateTimeFormat('en-u-hc-h11', { hour: 'numeric' }).resolvedOptions().hour12, true)
shouldBe(Intl.DateTimeFormat('en-u-hc-h11', { hour: 'numeric', timeZone: 'UTC' }).format(12 * 60 * 60 * 1000).slice(0, 1), '0')
shouldBe(Intl.DateTimeFormat('en-u-hc-h12', { hour: 'numeric' }).resolvedOptions().hourCycle, 'h12')
shouldBe(Intl.DateTimeFormat('en-u-hc-h12', { hour: 'numeric' }).resolvedOptions().hour12, true)
shouldBe(Intl.DateTimeFormat('en-u-hc-h12', { hour: 'numeric', timeZone: 'UTC' }).format(12 * 60 * 60 * 1000).slice(0, 2), '12')
shouldBe(Intl.DateTimeFormat('en-u-hc-h23', { hour: 'numeric' }).resolvedOptions().hourCycle, 'h23')
shouldBe(Intl.DateTimeFormat('en-u-hc-h23', { hour: 'numeric' }).resolvedOptions().hour12, false)
shouldBe(Intl.DateTimeFormat('en-u-hc-h23', { hour: 'numeric', timeZone: 'UTC' }).format(0), '00')
shouldBe(Intl.DateTimeFormat('en-u-hc-h24', { hour: 'numeric' }).resolvedOptions().hourCycle, 'h24')
shouldBe(Intl.DateTimeFormat('en-u-hc-h24', { hour: 'numeric' }).resolvedOptions().hour12, false)
shouldBe(Intl.DateTimeFormat('en-u-hc-h24', { hour: 'numeric', timeZone: 'UTC' }).format(0), '24')

// Tests multiple keys in extension.
shouldBe(Intl.DateTimeFormat('en-u-ca-islamic-umalqura-nu-arab', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '٣/١٤/١٤٣٧ AH');

shouldThrow(() => Intl.DateTimeFormat('en', { weekday: { toString() { throw new Error() } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { weekday: 'invalid' }), RangeError);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().weekday, undefined);
shouldBe(Intl.DateTimeFormat('en', { weekday:'narrow', month:'numeric', day:'numeric' }).resolvedOptions().weekday, 'narrow');
shouldBe(Intl.DateTimeFormat('en', { weekday:'narrow', month:'numeric', day:'numeric', timeZone: 'UTC' }).format(0), 'T, 1/1');
shouldBe(Intl.DateTimeFormat('en', { weekday:'short', month:'numeric', day:'numeric' }).resolvedOptions().weekday, 'short');
shouldBe(Intl.DateTimeFormat('en', { weekday:'short', month:'numeric', day:'numeric', timeZone: 'UTC' }).format(0), 'Thu, 1/1');
shouldBe(Intl.DateTimeFormat('en', { weekday:'long', month:'numeric', day:'numeric' }).resolvedOptions().weekday, 'long');
shouldBe(Intl.DateTimeFormat('en', { weekday:'long', month:'numeric', day:'numeric', timeZone: 'UTC' }).format(0), 'Thursday, 1/1');

shouldThrow(() => Intl.DateTimeFormat('en', { era: { toString() { throw new Error() } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { era: 'never' }), RangeError);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().day, undefined);
shouldBe(Intl.DateTimeFormat('en', { era:'narrow', year:'numeric' }).resolvedOptions().era, 'narrow');
shouldBe(Intl.DateTimeFormat('en', { era:'narrow', year:'numeric', timeZone: 'UTC' }).format(0), '1970 A');
shouldBe(Intl.DateTimeFormat('en', { era:'short', year:'numeric' }).resolvedOptions().era, 'short');
shouldBe(Intl.DateTimeFormat('en', { era:'short', year:'numeric', timeZone: 'UTC' }).format(0), '1970 AD');
shouldBe(Intl.DateTimeFormat('en', { era:'long', year:'numeric' }).resolvedOptions().era, 'long');
shouldBe(Intl.DateTimeFormat('en', { era:'long', year:'numeric', timeZone: 'UTC' }).format(0), '1970 Anno Domini');

shouldThrow(() => Intl.DateTimeFormat('en', { year: { toString() { throw new Error() } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { year: 'nope' }), RangeError);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().year, undefined);
shouldBe(Intl.DateTimeFormat('en', { era:'narrow', year:'2-digit' }).resolvedOptions().year, '2-digit');
shouldBe(Intl.DateTimeFormat('en', { era:'narrow', year:'2-digit', timeZone: 'UTC' }).format(0), '70 A');
shouldBe(Intl.DateTimeFormat('en', { era:'narrow', year:'numeric' }).resolvedOptions().year, 'numeric');
shouldBe(Intl.DateTimeFormat('en', { era:'narrow', year:'numeric', timeZone: 'UTC' }).format(0), '1970 A');

shouldThrow(() => Intl.DateTimeFormat('en', { month: { toString() { throw new Error() } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { month: 2 }), RangeError);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().month, undefined);
shouldBe(Intl.DateTimeFormat('en', { month:'2-digit', year:'numeric' }).resolvedOptions().month, '2-digit');
shouldBe(Intl.DateTimeFormat('en', { month:'2-digit', year:'numeric', timeZone: 'UTC' }).format(0), '01/1970');
shouldBe(Intl.DateTimeFormat('en', { month:'numeric', year:'numeric' }).resolvedOptions().month, 'numeric');
shouldBe(Intl.DateTimeFormat('en', { month:'numeric', year:'numeric', timeZone: 'UTC' }).format(0), '1/1970');
shouldBe(Intl.DateTimeFormat('en', { month:'narrow', year:'numeric' }).resolvedOptions().month, 'narrow');
shouldBe(Intl.DateTimeFormat('en', { month:'narrow', year:'numeric', timeZone: 'UTC' }).format(0), 'J 1970');
shouldBe(Intl.DateTimeFormat('en', { month:'short', year:'numeric' }).resolvedOptions().month, 'short');
shouldBe(Intl.DateTimeFormat('en', { month:'short', year:'numeric', timeZone: 'UTC' }).format(0), 'Jan 1970');
shouldBe(Intl.DateTimeFormat('en', { month:'long', year:'numeric' }).resolvedOptions().month, 'long');
shouldBe(Intl.DateTimeFormat('en', { month:'long', year:'numeric', timeZone: 'UTC' }).format(0), 'January 1970');

shouldThrow(() => Intl.DateTimeFormat('en', { day: { toString() { throw new Error() } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { day: '' }), RangeError);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().day, undefined);
shouldBe(Intl.DateTimeFormat('en', { month:'long', day:'2-digit' }).resolvedOptions().day, '2-digit');
shouldBe(Intl.DateTimeFormat('en', { month:'long', day:'2-digit', timeZone: 'UTC' }).format(0), 'January 01');
shouldBe(Intl.DateTimeFormat('en', { month:'long', day:'numeric' }).resolvedOptions().day, 'numeric');
shouldBe(Intl.DateTimeFormat('en', { month:'long', day:'numeric', timeZone: 'UTC' }).format(0), 'January 1');

shouldThrow(() => Intl.DateTimeFormat('en', { hour: { toString() { throw new Error() } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { hour: [] }), RangeError);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().hour, undefined);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'2-digit' }).resolvedOptions().hour, '2-digit');
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'2-digit', timeZone: 'UTC' }).format(0), '12:00 AM');
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().hour, 'numeric');
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZone: 'UTC' }).format(0), '12:00 AM');

shouldBe(Intl.DateTimeFormat('en').resolvedOptions().hour12, undefined);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().hourCycle, 'h12');
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().hour12, true);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZone: 'UTC' }).format(0), '12:00 AM');
shouldBe(Intl.DateTimeFormat('pt-BR', { minute:'2-digit', hour:'numeric' }).resolvedOptions().hourCycle, 'h23');
shouldBe(Intl.DateTimeFormat('pt-BR', { minute:'2-digit', hour:'numeric' }).resolvedOptions().hour12, false);
shouldBe(Intl.DateTimeFormat('pt-BR', { minute:'2-digit', hour:'numeric', timeZone: 'UTC' }).format(0), '0:00');
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'2-digit', hour12: false, timeZone: 'UTC' }).format(0), '00:00');
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'2-digit', hour12: true, timeZone: 'UTC' }).format(1e7), '02:46 AM');

shouldThrow(() => Intl.DateTimeFormat('en', { minute: { toString() { throw new Error() } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { minute: null }), RangeError);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().minute, undefined);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().minute, '2-digit');
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZone: 'UTC' }).format(0), '12:00 AM');
shouldBe(Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric' }).resolvedOptions().minute, '2-digit');
shouldBe(Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', timeZone: 'UTC' }).format(0), '12:00 AM');

shouldThrow(() => Intl.DateTimeFormat('en', { second: { toString() { throw new Error() } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { second: 'badvalue' }), RangeError);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().second, undefined);
shouldBe(Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', second:'2-digit' }).resolvedOptions().second, '2-digit');
shouldBe(Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', second:'2-digit', timeZone: 'UTC' }).format(0), '12:00:00 AM');
shouldBe(Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', second:'numeric' }).resolvedOptions().second, '2-digit');
shouldBe(Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', second:'numeric', timeZone: 'UTC' }).format(0), '12:00:00 AM');

shouldThrow(() => Intl.DateTimeFormat('en', { timeZoneName: { toString() { throw new Error() } } }), Error);
shouldThrow(() => Intl.DateTimeFormat('en', { timeZoneName: 'name' }), RangeError);
shouldBe(Intl.DateTimeFormat('en').resolvedOptions().timeZoneName, undefined);
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZoneName:'short' }).resolvedOptions().timeZoneName, 'short');
shouldBe(Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZoneName:'short', timeZone: 'America/Los_Angeles' }).format(0), '4:00 PM PST');
shouldBe(Intl.DateTimeFormat('pt-BR', { minute:'2-digit', hour:'numeric', timeZoneName:'long' }).resolvedOptions().timeZoneName, 'long');
shouldBe(Intl.DateTimeFormat('pt-BR', { minute:'2-digit', hour:'numeric', timeZoneName:'long', timeZone: 'america/sao_paulo' }).format(0), '21:00 Horário Padrão de Brasília');

let localesSample = [
    'ar', 'ar-SA', 'be', 'ca', 'cs', 'da', 'de', 'de-CH', 'en', 'en-AU', 'en-GB',
    'en-PH', 'en-US', 'el', 'es', 'es-MX', 'es-PR', 'fr', 'fr-CA', 'ga', 'hi-IN',
    'is', 'it', 'iw', 'ja', 'ko-KR', 'lt', 'lv', 'mk', 'ms', 'mt', 'nb', 'nl',
    'no', 'pl', 'pt', 'pt-BR', 'ro', 'ru', 'sk', 'sl', 'sr', 'sv', 'th', 'tr',
    'uk', 'vi', 'zh', 'zh-CN', 'zh-Hant-HK', 'zh-TW'
];
for (let locale of localesSample) {
    // The following subsets must be available for each locale:
    // weekday, year, month, day, hour, minute, second
    {
        let options = { weekday: 'short', year: 'numeric', month: 'short', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' };
        let resolved = Intl.DateTimeFormat(locale, options).resolvedOptions();
        shouldBe(Object.keys(options).every(option => resolved[option] != null), true);
    }
    shouldBe(typeof Intl.DateTimeFormat(locale, { weekday: 'short', year: 'numeric', month: 'short', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' }).format(), 'string');
    // weekday, year, month, day
    {
        let options = { weekday: 'short', year: 'numeric', month: 'short', day: 'numeric' };
        let resolved = Intl.DateTimeFormat(locale, options).resolvedOptions();
        shouldBe(Object.keys(options).every(option => resolved[option] != null), true);
    }
    shouldBe(typeof Intl.DateTimeFormat(locale, { weekday: 'short', year: 'numeric', month: 'short', day: 'numeric' }).format(), 'string');
    // year, month, day
    {
        let options = { year: 'numeric', month: 'long', day: 'numeric' };
        let resolved = Intl.DateTimeFormat(locale, options).resolvedOptions();
        shouldBe(Object.keys(options).every(option => resolved[option] != null), true);
    }
    shouldBe(typeof Intl.DateTimeFormat(locale, { year: 'numeric', month: 'long', day: 'numeric' }).format(), 'string');
    // year, month
    {
        let options = { year: 'numeric', month: 'long' };
        let resolved = Intl.DateTimeFormat(locale, options).resolvedOptions();
        shouldBe(Object.keys(options).every(option => resolved[option] != null), true);
    }
    shouldBe(typeof Intl.DateTimeFormat(locale, { year: 'numeric', month: 'long' }).format(), 'string');
    // month, day
    {
        let options = { month: 'long', day: 'numeric' };
        let resolved = Intl.DateTimeFormat(locale, options).resolvedOptions();
        shouldBe(Object.keys(options).every(option => resolved[option] != null), true);
    }
    shouldBe(typeof Intl.DateTimeFormat(locale, { month: 'long', day: 'numeric' }).format(), 'string');
    // hour, minute, second
    {
        let options = { hour: 'numeric', minute: 'numeric', second: 'numeric' };
        let resolved = Intl.DateTimeFormat(locale, options).resolvedOptions();
        shouldBe(Object.keys(options).every(option => resolved[option] != null), true);
    }
    shouldBe(typeof Intl.DateTimeFormat(locale, { hour: 'numeric', minute: 'numeric', second: 'numeric' }).format(), 'string');
    // hour, minute
    {
        let options = { hour: 'numeric', minute: 'numeric' };
        let resolved = Intl.DateTimeFormat(locale, options).resolvedOptions();
        shouldBe(Object.keys(options).every(option => resolved[option] != null), true);
    }
    shouldBe(typeof Intl.DateTimeFormat(locale, { hour: 'numeric', minute: 'numeric' }).format(), 'string');
}

// Legacy compatibility with ECMA-402 1.0
{
    let legacy = Object.create(Intl.DateTimeFormat.prototype);
    let incompat = {};
    shouldBe(Intl.DateTimeFormat.apply(legacy), legacy);
    shouldBe(Intl.DateTimeFormat.call(legacy, 'en-u-nu-arab', { timeZone: 'America/Los_Angeles' }).format(1451099872641), '١٢/٢٥/٢٠١٥');
    shouldBe(Intl.DateTimeFormat.apply(incompat) !== incompat, true);
}

// ECMA-402 4th edition 15.4 Intl.DateTimeFormat.prototype.formatToParts
shouldBe(Intl.DateTimeFormat.prototype.formatToParts instanceof Function, true);
shouldBe(Intl.DateTimeFormat.prototype.formatToParts.length, 1);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'formatToParts').writable, true);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'formatToParts').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'formatToParts').configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'formatToParts').get, undefined);
shouldBe(Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'formatToParts').set, undefined);

// Throws on non-finite or non-number
shouldThrow(() => new Intl.DateTimeFormat().formatToParts({}), RangeError);
shouldThrow(() => new Intl.DateTimeFormat().formatToParts(NaN), RangeError);
shouldThrow(() => new Intl.DateTimeFormat().formatToParts(Infinity), RangeError);
shouldThrow(() => new Intl.DateTimeFormat().formatToParts(-Infinity), RangeError);
shouldThrow(() => new Intl.DateTimeFormat().formatToParts(new Date(NaN)), RangeError);

shouldBe(
    JSON.stringify(
        Intl.DateTimeFormat('pt-BR', {
            hour: 'numeric', minute: 'numeric', second: 'numeric',
            year: 'numeric', month: 'numeric', day: 'numeric',
            timeZoneName: 'short', era: 'short', timeZone: 'America/Sao_Paulo'
        }).formatToParts(0).filter((part) => (part.type !== 'literal'))
    ),
    JSON.stringify([
        { type: 'day', value: '31' },
        { type: 'month', value: '12' },
        { type: 'year', value: '1969' },
        { type: 'era', value: 'd.C.' },
        { type: 'hour', value: '21' },
        { type: 'minute', value: '00' },
        { type: 'second', value: '00' },
        { type: 'timeZoneName', value: 'BRT' }
    ])
);

for (let locale of localesSample) {
  // The following subsets must be available for each locale:
  // weekday, year, month, day, hour, minute, second
  shouldBe(Intl.DateTimeFormat(locale, { weekday: 'short', year: 'numeric', month: 'short', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' }).formatToParts() instanceof Array, true);
  // weekday, year, month, day
  shouldBe(Intl.DateTimeFormat(locale, { weekday: 'short', year: 'numeric', month: 'short', day: 'numeric' }).formatToParts() instanceof Array, true);
  // year, month, day
  shouldBe(Intl.DateTimeFormat(locale, { year: 'numeric', month: 'long', day: 'numeric' }).formatToParts() instanceof Array, true);
  // year, month
  shouldBe(Intl.DateTimeFormat(locale, { year: 'numeric', month: 'long' }).formatToParts() instanceof Array, true);
  // month, day
  shouldBe(Intl.DateTimeFormat(locale, { month: 'long', day: 'numeric' }).formatToParts() instanceof Array, true);
  // hour, minute, second
  shouldBe(Intl.DateTimeFormat(locale, { hour: 'numeric', minute: 'numeric', second: 'numeric' }).formatToParts() instanceof Array, true);
  // hour, minute
  shouldBe(Intl.DateTimeFormat(locale, { hour: 'numeric', minute: 'numeric' }).formatToParts() instanceof Array, true);
}

// Exceed the 32 character default buffer size
shouldBe(
    JSON.stringify(
        Intl.DateTimeFormat('en-US', {
            hour: 'numeric', minute: 'numeric', second: 'numeric',
            year: 'numeric', month: 'long', day: 'numeric', weekday: 'long',
            timeZoneName: 'long', era: 'long', timeZone: 'America/Los_Angeles'
        }).formatToParts(0)
    ),
    JSON.stringify([
        { type: 'weekday', value: 'Wednesday' },
        { type: 'literal', value: ', ' },
        { type: 'month', value: 'December' },
        { type: 'literal', value: ' ' },
        { type: 'day', value: '31' },
        { type: 'literal', value: ', ' },
        { type: 'year', value: '1969' },
        { type: 'literal', value: ' ' },
        { type: 'era', value: 'Anno Domini' },
        { type: 'literal', value: ', ' },
        { type: 'hour', value: '4' },
        { type: 'literal', value: ':' },
        { type: 'minute', value: '00' },
        { type: 'literal', value: ':' },
        { type: 'second', value: '00' },
        { type: 'literal', value: ' ' },
        { type: 'dayPeriod', value: 'PM' },
        { type: 'literal', value: ' ' },
        { type: 'timeZoneName', value: 'Pacific Standard Time' }
    ])
);

// Tests for relativeYear and yearName
const parts = JSON.stringify([
    { type: 'relatedYear', value: '1969' },
    { type: 'yearName', value: '己酉' },
    { type: 'literal', value: '年' }
]);
shouldBe(Intl.DateTimeFormat('zh-u-ca-chinese', { era: 'short', year: 'numeric' }).format(0), '1969己酉年');
shouldBe(Intl.DateTimeFormat('zh', { era: 'short', year: 'numeric', calendar: 'chinese' }).format(0), '1969己酉年');
shouldBe(JSON.stringify(Intl.DateTimeFormat('zh-u-ca-chinese', { era: 'short', year: 'numeric' }).formatToParts(0)), parts);
shouldBe(JSON.stringify(Intl.DateTimeFormat('zh', { era: 'short', year: 'numeric', calendar: 'chinese' }).formatToParts(0)), parts);
