description("This test checks the behavior of Intl.DateTimeFormat as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

// 12.1 The Intl.DateTimeFormat Constructor

// The Intl.DateTimeFormat constructor is a standard built-in property of the Intl object.
shouldBeType("Intl.DateTimeFormat", "Function");

// 12.1.2 Intl.DateTimeFormat([ locales [, options]])
shouldBeType("Intl.DateTimeFormat()", "Intl.DateTimeFormat");
shouldBeType("Intl.DateTimeFormat.call({})", "Intl.DateTimeFormat");
shouldBeType("new Intl.DateTimeFormat()", "Intl.DateTimeFormat");

// Subclassable
var classPrefix = "class DerivedDateTimeFormat extends Intl.DateTimeFormat {};";
shouldBeTrue(classPrefix + "(new DerivedDateTimeFormat) instanceof DerivedDateTimeFormat");
shouldBeTrue(classPrefix + "(new DerivedDateTimeFormat) instanceof Intl.DateTimeFormat");
shouldBeTrue(classPrefix + "new DerivedDateTimeFormat().format(0).length > 0");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(new DerivedDateTimeFormat) === DerivedDateTimeFormat.prototype");
shouldBeTrue(classPrefix + "Object.getPrototypeOf(Object.getPrototypeOf(new DerivedDateTimeFormat)) === Intl.DateTimeFormat.prototype");

// 12.2 Properties of the Intl.DateTimeFormat Constructor

// length property (whose value is 0)
shouldBe("Intl.DateTimeFormat.length", "0");

// 12.2.1 Intl.DateTimeFormat.prototype

// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat, 'prototype').writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat, 'prototype').enumerable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat, 'prototype').configurable");

// 12.2.2 Intl.DateTimeFormat.supportedLocalesOf (locales [, options ])

// The value of the length property of the supportedLocalesOf method is 1.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf.length", "1");

// Returns SupportedLocales
shouldBeType("Intl.DateTimeFormat.supportedLocalesOf()", "Array");
// Doesn't care about `this`.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf.call(null, 'en')", "[ 'en' ]");
shouldBe("Intl.DateTimeFormat.supportedLocalesOf.call({}, 'en')", "[ 'en' ]");
shouldBe("Intl.DateTimeFormat.supportedLocalesOf.call(1, 'en')", "[ 'en' ]");
// Ignores non-object, non-string list.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf(9)", "[]");
// Makes an array of tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf('en')", "[ 'en' ]");
// Handles array-like objects with holes.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })", "[ 'es', 'en', 'de' ]");
// Deduplicates tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])", "[ 'en', 'pt', 'es' ]");
// Canonicalizes tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf('En-laTn-us-variant2-variant1-1abc-U-ko-tRue-A-aa-aaa-x-RESERVED')", "[ 'en-Latn-US-variant2-variant1-1abc-a-aa-aaa-u-ko-true-x-reserved' ]");
// Replaces outdated tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf('no-bok')", "[ 'nb' ]");
// Doesn't throw, but ignores private tags.
shouldBe("Intl.DateTimeFormat.supportedLocalesOf('x-some-thing')", "[]");
// Throws on problems with length, get, or toString.
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf(Object.create(null, { length: { get() { throw Error('a') } } }))", "'Error: a'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw Error('b') } } }))", "'Error: b'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf([ { toString() { throw Error('c') } } ])", "'Error: c'");
// Throws on bad tags.
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf([ 5 ])", "'TypeError: locale value must be a string or object'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('')", "'RangeError: invalid language tag: '");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('a')", "'RangeError: invalid language tag: a'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('abcdefghij')", "'RangeError: invalid language tag: abcdefghij'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('#$')", "'RangeError: invalid language tag: #$'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-@-abc')", "'RangeError: invalid language tag: en-@-abc'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-u')", "'RangeError: invalid language tag: en-u'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-u-kn-true-u-ko-true')", "'RangeError: invalid language tag: en-u-kn-true-u-ko-true'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-x')", "'RangeError: invalid language tag: en-x'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-*')", "'RangeError: invalid language tag: en-*'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en-')", "'RangeError: invalid language tag: en-'");
shouldThrow("Intl.DateTimeFormat.supportedLocalesOf('en--US')", "'RangeError: invalid language tag: en--US'");

// 12.3 Properties of the Intl.DateTimeFormat Prototype Object

// The value of Intl.DateTimeFormat.prototype.constructor is %DateTimeFormat%.
shouldBe("Intl.DateTimeFormat.prototype.constructor", "Intl.DateTimeFormat");

// 12.3.3 Intl.DateTimeFormat.prototype.format

// This named accessor property returns a function that formats a date according to the effective locale and the formatting options of this DateTimeFormat object.
shouldBeType("Intl.DateTimeFormat.prototype.format", "Function");

// The value of the [[Get]] attribute is a function
shouldBeType("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').get", "Function");

// The value of the [[Set]] attribute is undefined.
shouldBe("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').set", "undefined");

// Match Firefox where unspecifed.
shouldBeFalse("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format').configurable");

// The value of F’s length property is 1.
shouldBe("Intl.DateTimeFormat.prototype.format.length", "1");

// Throws on non-DateTimeFormat this.
shouldThrow("Object.defineProperty({}, 'format', Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, 'format')).format", "'TypeError: Intl.DateTimeFormat.prototype.format called on value that\\'s not an object initialized as a DateTimeFormat'");

// The format function is unique per instance.
shouldBeTrue("Intl.DateTimeFormat.prototype.format !== Intl.DateTimeFormat().format");
shouldBeTrue("new Intl.DateTimeFormat().format !== new Intl.DateTimeFormat().format");

// 12.3.4 DateTime Format Functions

// 1. Let dtf be the this value.
// 2. Assert: Type(dtf) is Object and dtf has an [[initializedDateTimeFormat]] internal slot whose value is true.
// This should not be reachable, since format is bound to an initialized datetimeformat.

// 3. If date is not provided or is undefined, then
// a. Let x be %Date_now%().
// 4. Else
// a. Let x be ToNumber(date).
// b. ReturnIfAbrupt(x).
shouldThrow("Intl.DateTimeFormat.prototype.format({ valueOf() { throw Error('4b') } })", "'Error: 4b'");

// 12.3.4 FormatDateTime abstract operation

// 1. If x is not a finite Number, then throw a RangeError exception.
shouldThrow("Intl.DateTimeFormat.prototype.format(Infinity)", "'RangeError: date value is not finite in DateTimeFormat format()'");

// Format is bound, so calling with alternate "this" has no effect.
shouldBe("Intl.DateTimeFormat.prototype.format.call(null, 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("Intl.DateTimeFormat.prototype.format.call(Intl.DateTimeFormat('ar'), 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("Intl.DateTimeFormat.prototype.format.call(5, 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("new Intl.DateTimeFormat().format.call(null, 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("new Intl.DateTimeFormat().format.call(Intl.DateTimeFormat('ar'), 0)", "Intl.DateTimeFormat().format(0)");
shouldBe("new Intl.DateTimeFormat().format.call(5, 0)", "Intl.DateTimeFormat().format(0)");

shouldBeTrue("typeof Intl.DateTimeFormat.prototype.format() === 'string'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'America/Denver' }).format(new Date(1451099872641))", "'12/25/2015'");

// 12.3.5 Intl.DateTimeFormat.prototype.resolvedOptions ()

shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions.length", "0");

// Returns a new object whose properties and attributes are set as if constructed by an object literal.
shouldBeType("Intl.DateTimeFormat.prototype.resolvedOptions()", "Object");

// The Intl.DateTimeFormat prototype object is itself an %DateTimeFormat% instance, whose internal slots are set as if it had been constructed by the expression Construct(%DateTimeFormat%).
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().locale", "new Intl.DateTimeFormat().resolvedOptions().locale");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().timeZone", "new Intl.DateTimeFormat().resolvedOptions().timeZone");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().calendar", "new Intl.DateTimeFormat().resolvedOptions().calendar");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().numberingSystem", "new Intl.DateTimeFormat().resolvedOptions().numberingSystem");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().weekday", "new Intl.DateTimeFormat().resolvedOptions().weekday");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().era", "new Intl.DateTimeFormat().resolvedOptions().era");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().year", "new Intl.DateTimeFormat().resolvedOptions().year");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().month", "new Intl.DateTimeFormat().resolvedOptions().month");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().day", "new Intl.DateTimeFormat().resolvedOptions().day");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().hour", "new Intl.DateTimeFormat().resolvedOptions().hour");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().hour12", "new Intl.DateTimeFormat().resolvedOptions().hour12");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().minute", "new Intl.DateTimeFormat().resolvedOptions().minute");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().second", "new Intl.DateTimeFormat().resolvedOptions().second");
shouldBe("Intl.DateTimeFormat.prototype.resolvedOptions().timeZoneName", "new Intl.DateTimeFormat().resolvedOptions().timeZoneName");

// Returns a new object each time.
shouldBeFalse("Intl.DateTimeFormat.prototype.resolvedOptions() === Intl.DateTimeFormat.prototype.resolvedOptions()");

// Throws on non-DateTimeFormat this.
shouldThrow("Intl.DateTimeFormat.prototype.resolvedOptions.call(5)", "'TypeError: Intl.DateTimeFormat.prototype.resolvedOptions called on value that\\'s not an object initialized as a DateTimeFormat'");

shouldThrow("Intl.DateTimeFormat('$')", "'RangeError: invalid language tag: $'");
shouldThrow("Intl.DateTimeFormat('en', null)", '"TypeError: null is not an object (evaluating \'Intl.DateTimeFormat(\'en\', null)\')"');

// Defaults to en-US locale in test runner
shouldBe("Intl.DateTimeFormat().resolvedOptions().locale", "'en-US'");

// Defaults to month, day, year.
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().weekday", "undefined");
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().era", "undefined");
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().month", "'numeric'");
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().day", "'numeric'");
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().year", "'numeric'");
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().hour", "undefined");
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().hour12", "undefined");
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().minute", "undefined");
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().second", "undefined");
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().timeZoneName", "undefined");

// Locale-sensitive format().
shouldBe("Intl.DateTimeFormat('ar', { timeZone: 'America/Denver' }).format(1451099872641)", "'٢٥‏/١٢‏/٢٠١٥'");
shouldBe("Intl.DateTimeFormat('de', { timeZone: 'America/Denver' }).format(1451099872641)", "'25.12.2015'");
shouldBe("Intl.DateTimeFormat('ja', { timeZone: 'America/Denver' }).format(1451099872641)", "'2015/12/25'");
shouldBe("Intl.DateTimeFormat('pt', { timeZone: 'America/Denver' }).format(1451099872641)", "'25/12/2015'");

shouldThrow("Intl.DateTimeFormat('en', { localeMatcher: { toString() { throw 'nope' } } })", "'nope'");
shouldThrow("Intl.DateTimeFormat('en', { localeMatcher:'bad' })", '\'RangeError: localeMatcher must be either "lookup" or "best fit"\'');
shouldNotThrow("Intl.DateTimeFormat('en', { localeMatcher:'lookup' })");
shouldNotThrow("Intl.DateTimeFormat('en', { localeMatcher:'best fit' })");

shouldThrow("Intl.DateTimeFormat('en', { formatMatcher: { toString() { throw 'nope' } } })", "'nope'");
shouldThrow("Intl.DateTimeFormat('en', { formatMatcher:'bad' })", '\'RangeError: formatMatcher must be either "basic" or "best fit"\'');
shouldNotThrow("Intl.DateTimeFormat('en', { formatMatcher:'basic' })");
shouldNotThrow("Intl.DateTimeFormat('en', { formatMatcher:'best fit' })");

shouldThrow("Intl.DateTimeFormat('en', { timeZone: 'nowhere/bogus' })", "'RangeError: invalid time zone: nowhere/bogus'");
shouldThrow("Intl.DateTimeFormat('en', { timeZone: { toString() { throw 'nope' } } })", "'nope'");

// Time zone is case insensitive.
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'america/denver' }).resolvedOptions().timeZone", "'America/Denver'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'AMERICA/LOS_ANGELES' }).resolvedOptions().timeZone", "'America/Los_Angeles'");

// Default time zone is a valid canonical time zone.
shouldBe("Intl.DateTimeFormat('en', { timeZone: Intl.DateTimeFormat().resolvedOptions().timeZone }).resolvedOptions().timeZone", "Intl.DateTimeFormat().resolvedOptions().timeZone");

// Time zone is canonicalized for obsolete links in IANA tz backward file.
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Australia/ACT' }).resolvedOptions().timeZone", "'Australia/Sydney'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Australia/North' }).resolvedOptions().timeZone", "'Australia/Darwin'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Australia/South' }).resolvedOptions().timeZone", "'Australia/Adelaide'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Australia/West' }).resolvedOptions().timeZone", "'Australia/Perth'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Brazil/East' }).resolvedOptions().timeZone", "'America/Sao_Paulo'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Brazil/West' }).resolvedOptions().timeZone", "'America/Manaus'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Canada/Atlantic' }).resolvedOptions().timeZone", "'America/Halifax'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Canada/Central' }).resolvedOptions().timeZone", "'America/Winnipeg'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Canada/Eastern' }).resolvedOptions().timeZone", "'America/Toronto'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Canada/Mountain' }).resolvedOptions().timeZone", "'America/Edmonton'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Canada/Pacific' }).resolvedOptions().timeZone", "'America/Vancouver'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'GB' }).resolvedOptions().timeZone", "'Europe/London'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'GMT+0' }).resolvedOptions().timeZone", "'UTC'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'GMT-0' }).resolvedOptions().timeZone", "'UTC'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'GMT0' }).resolvedOptions().timeZone", "'UTC'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Greenwich' }).resolvedOptions().timeZone", "'UTC'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'UCT' }).resolvedOptions().timeZone", "'UTC'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'US/Central' }).resolvedOptions().timeZone", "'America/Chicago'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'US/Eastern' }).resolvedOptions().timeZone", "'America/New_York'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'US/Michigan' }).resolvedOptions().timeZone", "'America/Detroit'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'US/Mountain' }).resolvedOptions().timeZone", "'America/Denver'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'US/Pacific' }).resolvedOptions().timeZone", "'America/Los_Angeles'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'UTC' }).resolvedOptions().timeZone", "'UTC'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Universal' }).resolvedOptions().timeZone", "'UTC'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Zulu' }).resolvedOptions().timeZone", "'UTC'");

// Timezone-sensitive format().
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'12/25/2015'");
shouldBe("Intl.DateTimeFormat('en', { timeZone: 'Pacific/Auckland' }).format(1451099872641)", "'12/26/2015'");

// Gets default calendar and numberingSystem from locale.
shouldBe("Intl.DateTimeFormat('ar-sa').resolvedOptions().locale", "'ar-SA'");
shouldBe("Intl.DateTimeFormat('fa-IR').resolvedOptions().calendar", "'persian'");
shouldBe("Intl.DateTimeFormat('ar').resolvedOptions().numberingSystem", "'arab'");

shouldBe("Intl.DateTimeFormat('en', { calendar:'dangi' }).resolvedOptions().calendar", "'gregorian'");
shouldBe("Intl.DateTimeFormat('en-u-ca-bogus').resolvedOptions().locale", "'en'");
shouldBe("Intl.DateTimeFormat('en-u-ca-bogus').resolvedOptions().calendar", "'gregorian'");
shouldBe("Intl.DateTimeFormat('en-u-ca-buddhist').resolvedOptions().locale", "'en-u-ca-buddhist'");
shouldBe("Intl.DateTimeFormat('en-u-ca-buddhist').resolvedOptions().calendar", "'buddhist'");
shouldBe("Intl.DateTimeFormat('en-u-ca-chinese').resolvedOptions().calendar", "'chinese'");
shouldBe("Intl.DateTimeFormat('en-u-ca-coptic').resolvedOptions().calendar", "'coptic'");
shouldBe("Intl.DateTimeFormat('en-u-ca-dangi').resolvedOptions().calendar", "'dangi'");
shouldBe("Intl.DateTimeFormat('en-u-ca-ethioaa').resolvedOptions().calendar", "'ethiopic-amete-alem'");
shouldBe("Intl.DateTimeFormat('en-u-ca-ethiopic').resolvedOptions().calendar", "'ethiopic'");
shouldBe("Intl.DateTimeFormat('ar-SA-u-ca-gregory').resolvedOptions().calendar", "'gregorian'");
shouldBe("Intl.DateTimeFormat('en-u-ca-hebrew').resolvedOptions().calendar", "'hebrew'");
shouldBe("Intl.DateTimeFormat('en-u-ca-indian').resolvedOptions().calendar", "'indian'");
shouldBe("Intl.DateTimeFormat('en-u-ca-islamic').resolvedOptions().calendar", "'islamic'");
shouldBe("Intl.DateTimeFormat('en-u-ca-islamicc').resolvedOptions().calendar", "'islamic-civil'");
shouldBe("Intl.DateTimeFormat('en-u-ca-ISO8601').resolvedOptions().calendar", "'iso8601'");
shouldBe("Intl.DateTimeFormat('en-u-ca-japanese').resolvedOptions().calendar", "'japanese'");
shouldBe("Intl.DateTimeFormat('en-u-ca-persian').resolvedOptions().calendar", "'persian'");
shouldBe("Intl.DateTimeFormat('en-u-ca-roc').resolvedOptions().calendar", "'roc'");
// FIXME: https://github.com/tc39/ecma402/issues/59
// shouldBe("Intl.DateTimeFormat('en-u-ca-ethiopic-amete-alem').resolvedOptions().calendar", "'ethioaa'");
// shouldBe("Intl.DateTimeFormat('en-u-ca-islamic-umalqura').resolvedOptions().calendar", "'islamic-umalqura'");
// shouldBe("Intl.DateTimeFormat('en-u-ca-islamic-tbla').resolvedOptions().calendar", "'islamic-tbla'");
// shouldBe("Intl.DateTimeFormat('en-u-ca-islamic-civil').resolvedOptions().calendar", "'islamic-civil'");
// shouldBe("Intl.DateTimeFormat('en-u-ca-islamic-rgsa').resolvedOptions().calendar", "'islamic-rgsa'");

// Calendar-sensitive format().
shouldBe("Intl.DateTimeFormat('en-u-ca-buddhist', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'12/25/2558'");
shouldBe("Intl.DateTimeFormat('en-u-ca-chinese', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'11/15/32'");
shouldBe("Intl.DateTimeFormat('en-u-ca-coptic', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'4/15/1732'");
shouldBe("Intl.DateTimeFormat('en-u-ca-dangi', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'11/15/32'");
shouldBe("Intl.DateTimeFormat('en-u-ca-ethioaa', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'4/15/7508'");
shouldBe("Intl.DateTimeFormat('en-u-ca-ethiopic', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'4/15/2008'");
shouldBe("Intl.DateTimeFormat('en-u-ca-gregory', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'12/25/2015'");
shouldBe("Intl.DateTimeFormat('en-u-ca-hebrew', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'4/13/5776'");
shouldBe("Intl.DateTimeFormat('en-u-ca-indian', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'10/4/1937'");
shouldBe("Intl.DateTimeFormat('en-u-ca-islamic', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'3/14/1437'");
shouldBe("Intl.DateTimeFormat('en-u-ca-islamicc', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'3/13/1437'");
shouldBe("Intl.DateTimeFormat('en-u-ca-ISO8601', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'12/25/2015'");
shouldBe("Intl.DateTimeFormat('en-u-ca-japanese', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'12/25/27'");
shouldBe("Intl.DateTimeFormat('en-u-ca-persian', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'10/4/1394'");
shouldBe("Intl.DateTimeFormat('en-u-ca-roc', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'12/25/104'");

shouldBe("Intl.DateTimeFormat('en', { numberingSystem:'gujr' }).resolvedOptions().numberingSystem", "'latn'");
shouldBe("Intl.DateTimeFormat('en-u-nu-bogus').resolvedOptions().locale", "'en'");
shouldBe("Intl.DateTimeFormat('en-u-nu-bogus').resolvedOptions().numberingSystem", "'latn'");
shouldBe("Intl.DateTimeFormat('en-u-nu-latn').resolvedOptions().numberingSystem", "'latn'");
shouldBe("Intl.DateTimeFormat('en-u-nu-arab').resolvedOptions().locale", "'en-u-nu-arab'");

let numberingSystems = [
  "arab", "arabext", "armn", "armnlow", "bali", "beng", "cham", "deva", "ethi",
  "fullwide", "geor", "grek", "greklow", "gujr", "guru", "hanidec",
  "hans", "hansfin", "hant", "hantfin", "hebr", "java", "jpan", "jpanfin",
  "kali", "khmr", "knda", "lana", "lanatham", "laoo", "latn", "lepc", "limb",
  "mlym", "mong", "mtei", "mymr", "mymrshan", "nkoo", "olck", "orya", "roman",
  "romanlow", "saur", "sund", "talu", "taml", "tamldec", "telu", "thai", "tibt",
  "vaii"
]
for (let numberingSystem of numberingSystems) {
  shouldBe(`Intl.DateTimeFormat('en-u-nu-${numberingSystem}').resolvedOptions().numberingSystem`, `'${numberingSystem}'`);
}

// Numbering system sensitive format().
shouldBe("Intl.DateTimeFormat('en-u-nu-arab', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'١٢/٢٥/٢٠١٥'");
shouldBe("Intl.DateTimeFormat('en-u-nu-arabext', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'۱۲/۲۵/۲۰۱۵'");
shouldBe("Intl.DateTimeFormat('en-u-nu-armn', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'ԺԲ/ԻԵ/ՍԺԵ'");
shouldBe("Intl.DateTimeFormat('en-u-nu-armnlow', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'ժբ/իե/սժե'");
shouldBe("Intl.DateTimeFormat('en-u-nu-bali', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'᭑᭒/᭒᭕/᭒᭐᭑᭕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-beng', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'১২/২৫/২০১৫'");
shouldBe("Intl.DateTimeFormat('en-u-nu-cham', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'꩑꩒/꩒꩕/꩒꩐꩑꩕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-deva', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'१२/२५/२०१५'");
shouldBe("Intl.DateTimeFormat('en-u-nu-ethi', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'፲፪/፳፭/፳፻፲፭'");
shouldBe("Intl.DateTimeFormat('en-u-nu-fullwide', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'１２/２５/２０１５'");
shouldBe("Intl.DateTimeFormat('en-u-nu-geor', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'იბ/კე/ჩიე'");
shouldBe("Intl.DateTimeFormat('en-u-nu-grek', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'ΙΒ´/ΚΕ´/͵ΒΙΕ´'");
shouldBe("Intl.DateTimeFormat('en-u-nu-greklow', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'ιβ´/κε´/͵βιε´'");
shouldBe("Intl.DateTimeFormat('en-u-nu-gujr', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'૧૨/૨૫/૨૦૧૫'");
shouldBe("Intl.DateTimeFormat('en-u-nu-guru', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'੧੨/੨੫/੨੦੧੫'");
shouldBe("Intl.DateTimeFormat('en-u-nu-hanidec', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'一二/二五/二〇一五'");
shouldBe("Intl.DateTimeFormat('en-u-nu-hans', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'十二/二十五/二千零一十五'");
shouldBe("Intl.DateTimeFormat('en-u-nu-hansfin', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'拾贰/贰拾伍/贰仟零壹拾伍'");
shouldBe("Intl.DateTimeFormat('en-u-nu-hant', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'十二/二十五/二千零一十五'");
shouldBe("Intl.DateTimeFormat('en-u-nu-hantfin', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'拾貳/貳拾伍/貳仟零壹拾伍'");
shouldBe("Intl.DateTimeFormat('en-u-nu-hebr', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'י״ב/כ״ה/ב׳ט״ו'");
shouldBe("Intl.DateTimeFormat('en-u-nu-java', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'꧑꧒/꧒꧕/꧒꧐꧑꧕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-jpan', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'十二/二十五/二千十五'");
shouldBe("Intl.DateTimeFormat('en-u-nu-jpanfin', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'拾弐/弐拾伍/弐千拾伍'");
shouldBe("Intl.DateTimeFormat('en-u-nu-kali', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'꤁꤂/꤂꤅/꤂꤀꤁꤅'");
shouldBe("Intl.DateTimeFormat('en-u-nu-khmr', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'១២/២៥/២០១៥'");
shouldBe("Intl.DateTimeFormat('en-u-nu-knda', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'೧೨/೨೫/೨೦೧೫'");
shouldBe("Intl.DateTimeFormat('en-u-nu-lana', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'᪁᪂/᪂᪅/᪂᪀᪁᪅'");
shouldBe("Intl.DateTimeFormat('en-u-nu-lanatham', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'᪑᪒/᪒᪕/᪒᪐᪑᪕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-laoo', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'໑໒/໒໕/໒໐໑໕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-latn', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'12/25/2015'");
shouldBe("Intl.DateTimeFormat('en-u-nu-lepc', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'᱁᱂/᱂᱅/᱂᱀᱁᱅'");
shouldBe("Intl.DateTimeFormat('en-u-nu-limb', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'᥇᥈/᥈᥋/᥈᥆᥇᥋'");
shouldBe("Intl.DateTimeFormat('en-u-nu-mlym', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'൧൨/൨൫/൨൦൧൫'");
shouldBe("Intl.DateTimeFormat('en-u-nu-mong', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'᠑᠒/᠒᠕/᠒᠐᠑᠕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-mtei', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'꯱꯲/꯲꯵/꯲꯰꯱꯵'");
shouldBe("Intl.DateTimeFormat('en-u-nu-mymr', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'၁၂/၂၅/၂၀၁၅'");
shouldBe("Intl.DateTimeFormat('en-u-nu-mymrshan', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'႑႒/႒႕/႒႐႑႕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-nkoo', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'߁߂/߂߅/߂߀߁߅'");
shouldBe("Intl.DateTimeFormat('en-u-nu-olck', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'᱑᱒/᱒᱕/᱒᱐᱑᱕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-orya', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'୧୨/୨୫/୨୦୧୫'");
shouldBe("Intl.DateTimeFormat('en-u-nu-roman', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'XII/XXV/MMXV'");
shouldBe("Intl.DateTimeFormat('en-u-nu-romanlow', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'xii/xxv/mmxv'");
shouldBe("Intl.DateTimeFormat('en-u-nu-saur', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'꣑꣒/꣒꣕/꣒꣐꣑꣕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-sund', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'᮱᮲/᮲᮵/᮲᮰᮱᮵'");
shouldBe("Intl.DateTimeFormat('en-u-nu-talu', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'᧑᧒/᧒᧕/᧒᧐᧑᧕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-taml', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'௰௨/௨௰௫/௨௲௰௫'");
shouldBe("Intl.DateTimeFormat('en-u-nu-tamldec', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'௧௨/௨௫/௨௦௧௫'");
shouldBe("Intl.DateTimeFormat('en-u-nu-telu', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'౧౨/౨౫/౨౦౧౫'");
shouldBe("Intl.DateTimeFormat('en-u-nu-thai', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'๑๒/๒๕/๒๐๑๕'");
shouldBe("Intl.DateTimeFormat('en-u-nu-tibt', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'༡༢/༢༥/༢༠༡༥'");
shouldBe("Intl.DateTimeFormat('en-u-nu-vaii', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'꘡꘢/꘢꘥/꘢꘠꘡꘥'");

shouldThrow("Intl.DateTimeFormat('en', { weekday: { toString() { throw 'weekday' } } })", "'weekday'");
shouldThrow("Intl.DateTimeFormat('en', { weekday:'invalid' })", '\'RangeError: weekday must be "narrow", "short", or "long"\'');
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().weekday", "undefined");
shouldBe("Intl.DateTimeFormat('en', { weekday:'narrow', month:'numeric', day:'numeric' }).resolvedOptions().weekday", "'narrow'");
shouldBe("Intl.DateTimeFormat('en', { weekday:'narrow', month:'numeric', day:'numeric', timeZone: 'UTC' }).format(0)", "'T, 1/1'");
shouldBe("Intl.DateTimeFormat('en', { weekday:'short', month:'numeric', day:'numeric' }).resolvedOptions().weekday", "'short'");
shouldBe("Intl.DateTimeFormat('en', { weekday:'short', month:'numeric', day:'numeric', timeZone: 'UTC' }).format(0)", "'Thu, 1/1'");
shouldBe("Intl.DateTimeFormat('en', { weekday:'long', month:'numeric', day:'numeric' }).resolvedOptions().weekday", "'long'");
shouldBe("Intl.DateTimeFormat('en', { weekday:'long', month:'numeric', day:'numeric', timeZone: 'UTC' }).format(0)", "'Thursday, 1/1'");

shouldThrow("Intl.DateTimeFormat('en', { era: { toString() { throw 'era' } } })", "'era'");
shouldThrow("Intl.DateTimeFormat('en', { era:'never' })", '\'RangeError: era must be "narrow", "short", or "long"\'');
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().day", "undefined");
shouldBe("Intl.DateTimeFormat('en', { era:'narrow', year:'numeric' }).resolvedOptions().era", "'narrow'");
shouldBe("Intl.DateTimeFormat('en', { era:'narrow', year:'numeric', timeZone: 'UTC' }).format(0)", "'1970 A'");
shouldBe("Intl.DateTimeFormat('en', { era:'short', year:'numeric' }).resolvedOptions().era", "'short'");
shouldBe("Intl.DateTimeFormat('en', { era:'short', year:'numeric', timeZone: 'UTC' }).format(0)", "'1970 AD'");
shouldBe("Intl.DateTimeFormat('en', { era:'long', year:'numeric' }).resolvedOptions().era", "'long'");
shouldBe("Intl.DateTimeFormat('en', { era:'long', year:'numeric', timeZone: 'UTC' }).format(0)", "'1970 Anno Domini'");

shouldThrow("Intl.DateTimeFormat('en', { year: { toString() { throw 'year' } } })", "'year'");
shouldThrow("Intl.DateTimeFormat('en', { year:'nope' })", '\'RangeError: year must be "2-digit" or "numeric"\'');
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().year", "undefined");
shouldBe("Intl.DateTimeFormat('en', { era:'narrow', year:'2-digit' }).resolvedOptions().year", "'2-digit'");
shouldBe("Intl.DateTimeFormat('en', { era:'narrow', year:'2-digit', timeZone: 'UTC' }).format(0)", "'70 A'");
shouldBe("Intl.DateTimeFormat('en', { era:'narrow', year:'numeric' }).resolvedOptions().year", "'numeric'");
shouldBe("Intl.DateTimeFormat('en', { era:'narrow', year:'numeric', timeZone: 'UTC' }).format(0)", "'1970 A'");

shouldThrow("Intl.DateTimeFormat('en', { month: { toString() { throw 'month' } } })", "'month'");
shouldThrow("Intl.DateTimeFormat('en', { month:2 })", '\'RangeError: month must be "2-digit", "numeric", "narrow", "short", or "long"\'');
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().month", "undefined");
shouldBe("Intl.DateTimeFormat('en', { month:'2-digit', year:'numeric' }).resolvedOptions().month", "'2-digit'");
shouldBe("Intl.DateTimeFormat('en', { month:'2-digit', year:'numeric', timeZone: 'UTC' }).format(0)", "'01/1970'");
shouldBe("Intl.DateTimeFormat('en', { month:'numeric', year:'numeric' }).resolvedOptions().month", "'numeric'");
shouldBe("Intl.DateTimeFormat('en', { month:'numeric', year:'numeric', timeZone: 'UTC' }).format(0)", "'1/1970'");
shouldBe("Intl.DateTimeFormat('en', { month:'narrow', year:'numeric' }).resolvedOptions().month", "'narrow'");
shouldBe("Intl.DateTimeFormat('en', { month:'narrow', year:'numeric', timeZone: 'UTC' }).format(0)", "'J 1970'");
shouldBe("Intl.DateTimeFormat('en', { month:'short', year:'numeric' }).resolvedOptions().month", "'short'");
shouldBe("Intl.DateTimeFormat('en', { month:'short', year:'numeric', timeZone: 'UTC' }).format(0)", "'Jan 1970'");
shouldBe("Intl.DateTimeFormat('en', { month:'long', year:'numeric' }).resolvedOptions().month", "'long'");
shouldBe("Intl.DateTimeFormat('en', { month:'long', year:'numeric', timeZone: 'UTC' }).format(0)", "'January 1970'");

shouldThrow("Intl.DateTimeFormat('en', { day: { toString() { throw 'day' } } })", "'day'");
shouldThrow("Intl.DateTimeFormat('en', { day:'' })", '\'RangeError: day must be "2-digit" or "numeric"\'');
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().day", "undefined");
shouldBe("Intl.DateTimeFormat('en', { month:'long', day:'2-digit' }).resolvedOptions().day", "'2-digit'");
shouldBe("Intl.DateTimeFormat('en', { month:'long', day:'2-digit', timeZone: 'UTC' }).format(0)", "'January 01'");
shouldBe("Intl.DateTimeFormat('en', { month:'long', day:'numeric' }).resolvedOptions().day", "'numeric'");
shouldBe("Intl.DateTimeFormat('en', { month:'long', day:'numeric', timeZone: 'UTC' }).format(0)", "'January 1'");

shouldThrow("Intl.DateTimeFormat('en', { hour: { toString() { throw 'hour' } } })", "'hour'");
shouldThrow("Intl.DateTimeFormat('en', { hour:[] })", '\'RangeError: hour must be "2-digit" or "numeric"\'');
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().hour", "undefined");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'2-digit' }).resolvedOptions().hour", "'numeric'");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'2-digit', timeZone: 'UTC' }).format(0)", "'12:00 AM'");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().hour", "'numeric'");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZone: 'UTC' }).format(0)", "'12:00 AM'");

shouldBe("Intl.DateTimeFormat('en').resolvedOptions().hour12", "undefined");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().hour12", "true");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZone: 'UTC' }).format(0)", "'12:00 AM'");
shouldBe("Intl.DateTimeFormat('pt-BR', { minute:'2-digit', hour:'numeric' }).resolvedOptions().hour12", "false");
shouldBe("Intl.DateTimeFormat('pt-BR', { minute:'2-digit', hour:'numeric', timeZone: 'UTC' }).format(0)", "'00:00'");

shouldThrow("Intl.DateTimeFormat('en', { minute: { toString() { throw 'minute' } } })", "'minute'");
shouldThrow("Intl.DateTimeFormat('en', { minute:null })", '\'RangeError: minute must be "2-digit" or "numeric"\'');
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().minute", "undefined");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric' }).resolvedOptions().minute", "'2-digit'");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZone: 'UTC' }).format(0)", "'12:00 AM'");
shouldBe("Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric' }).resolvedOptions().minute", "'2-digit'");
shouldBe("Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', timeZone: 'UTC' }).format(0)", "'12:00 AM'");

shouldThrow("Intl.DateTimeFormat('en', { second: { toString() { throw 'second' } } })", "'second'");
shouldThrow("Intl.DateTimeFormat('en', { second:'badvalue' })", '\'RangeError: second must be "2-digit" or "numeric"\'');
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().second", "undefined");
shouldBe("Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', second:'2-digit' }).resolvedOptions().second", "'2-digit'");
shouldBe("Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', second:'2-digit', timeZone: 'UTC' }).format(0)", "'12:00:00 AM'");
shouldBe("Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', second:'numeric' }).resolvedOptions().second", "'2-digit'");
shouldBe("Intl.DateTimeFormat('en', { minute:'numeric', hour:'numeric', second:'numeric', timeZone: 'UTC' }).format(0)", "'12:00:00 AM'");

shouldThrow("Intl.DateTimeFormat('en', { timeZoneName: { toString() { throw 'timeZoneName' } } })", "'timeZoneName'");
shouldThrow("Intl.DateTimeFormat('en', { timeZoneName:'name' })", '\'RangeError: timeZoneName must be "short" or "long"\'');
shouldBe("Intl.DateTimeFormat('en').resolvedOptions().timeZoneName", "undefined");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZoneName:'short' }).resolvedOptions().timeZoneName", "'short'");
shouldBe("Intl.DateTimeFormat('en', { minute:'2-digit', hour:'numeric', timeZoneName:'short', timeZone: 'UTC' }).format(0)", "'12:00 AM GMT'");
shouldBe("Intl.DateTimeFormat('pt-BR', { minute:'2-digit', hour:'numeric', timeZoneName:'long' }).resolvedOptions().timeZoneName", "'long'");
shouldBe("Intl.DateTimeFormat('pt-BR', { minute:'2-digit', hour:'numeric', timeZoneName:'long', timeZone: 'UTC' }).format(0)", "'00:00 GMT'")

let localesSample = [
  "ar", "ar-SA", "be", "ca", "cs", "da", "de", "de-CH", "en", "en-AU", "en-GB",
  "en-PH", "en-US", "el", "es", "es-MX", "es-PR", "fr", "fr-CA", "ga", "hi-IN",
  "is", "it", "iw", "ja", "ko-KR", "lt", "lv", "mk", "ms", "mt", "nb", "nl",
  "no", "pl", "pt", "pt-BR", "ro", "ru", "sk", "sl", "sr", "sv", "th", "tr",
  "uk", "vi", "zh", "zh-CN", "zh-Hant-HK", "zh-TW"
];
for (let locale of localesSample) {
  // The following subsets must be available for each locale:
  // weekday, year, month, day, hour, minute, second
  shouldBeTrue(`
    var options = { weekday: "short", year: "numeric", month: "short", day: "numeric", hour: "numeric", minute: "numeric", second: "numeric" };
    var resolved = Intl.DateTimeFormat("${locale}", options).resolvedOptions();
    Object.keys(options).every(option => resolved[option] != null)`);
  shouldBeTrue(`typeof Intl.DateTimeFormat("${locale}", { weekday: "short", year: "numeric", month: "short", day: "numeric", hour: "numeric", minute: "numeric", second: "numeric" }).format() === "string"`);
  // weekday, year, month, day
  shouldBeTrue(`
    var options = { weekday: "short", year: "numeric", month: "short", day: "numeric" };
    var resolved = Intl.DateTimeFormat("${locale}", options).resolvedOptions();
    Object.keys(options).every(option => resolved[option] != null)`);
  shouldBeTrue(`typeof Intl.DateTimeFormat("${locale}", { weekday: "short", year: "numeric", month: "short", day: "numeric" }).format() === "string"`);
  // year, month, day
  shouldBeTrue(`
    var options = { year: "numeric", month: "long", day: "numeric" };
    var resolved = Intl.DateTimeFormat("${locale}", options).resolvedOptions();
    Object.keys(options).every(option => resolved[option] != null)`);
  shouldBeTrue(`typeof Intl.DateTimeFormat("${locale}", { year: "numeric", month: "long", day: "numeric" }).format() === "string"`);
  // year, month
  shouldBeTrue(`
    var options = { year: "numeric", month: "long" };
    var resolved = Intl.DateTimeFormat("${locale}", options).resolvedOptions();
    Object.keys(options).every(option => resolved[option] != null)`);
  shouldBeTrue(`typeof Intl.DateTimeFormat("${locale}", { year: "numeric", month: "long" }).format() === "string"`);
  // month, day
  shouldBeTrue(`
    var options = { month: "long", day: "numeric" };
    var resolved = Intl.DateTimeFormat("${locale}", options).resolvedOptions();
    Object.keys(options).every(option => resolved[option] != null)`);
  shouldBeTrue(`typeof Intl.DateTimeFormat("${locale}", { month: "long", day: "numeric" }).format() === "string"`);
  // hour, minute, second
  shouldBeTrue(`
    var options = { hour: "numeric", minute: "numeric", second: "numeric" };
    var resolved = Intl.DateTimeFormat("${locale}", options).resolvedOptions();
    Object.keys(options).every(option => resolved[option] != null)`);
  shouldBeTrue(`typeof Intl.DateTimeFormat("${locale}", { hour: "numeric", minute: "numeric", second: "numeric" }).format() === "string"`);
  // hour, minute
  shouldBeTrue(`
    var options = { hour: "numeric", minute: "numeric" };
    var resolved = Intl.DateTimeFormat("${locale}", options).resolvedOptions();
    Object.keys(options).every(option => resolved[option] != null)`);
  shouldBeTrue(`typeof Intl.DateTimeFormat("${locale}", { hour: "numeric", minute: "numeric" }).format() === "string"`);
}

// Legacy compatibility with ECMA-402 1.0
let legacyInit = "var legacy = Object.create(Intl.DateTimeFormat.prototype);";
shouldBe(legacyInit + "Intl.DateTimeFormat.apply(legacy)", "legacy");
shouldBe(legacyInit + "Intl.DateTimeFormat.call(legacy, 'en-u-nu-arab', { timeZone: 'America/Los_Angeles' }).format(1451099872641)", "'١٢/٢٥/٢٠١٥'");
shouldNotBe("var incompat = {};Intl.DateTimeFormat.apply(incompat)", "incompat");
