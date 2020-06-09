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

// 8 The Intl Object

// The Intl object is a single ordinary object.
shouldBe(Intl instanceof Object, true);
shouldBe(typeof Intl, 'object');
shouldBe(Object.prototype.toString.call(Intl), '[object Object]');

// The value of the [[Prototype]] internal slot of the Intl object is the intrinsic object %ObjectPrototype%.
shouldBe(Object.getPrototypeOf(Intl), Object.prototype);

// The Intl object is not a function object.
// It does not have a [[Construct]] internal method; it is not possible to use the Intl object as a constructor with the new operator.
shouldThrow(() => new Intl, TypeError);

// The Intl object does not have a [[Call]] internal method; it is not possible to invoke the Intl object as a function.
shouldThrow(() => Intl(), TypeError);

// Has only the built-in Collator, DateTimeFormat, and NumberFormat, which are not enumerable.
shouldBe(Object.keys(Intl).length, 0);

// Is deletable, inferred from use of "Initial" in spec, consistent with other implementations.
var __Intl = Intl;
shouldBe(delete Intl, true);

function global() { return this; }
shouldBe('Intl' in global(), false);

Intl = __Intl;

// 8.2.1 Intl.getCanonicalLocales(locales)

// The value of the length property of the getCanonicalLocales method is 1.
shouldBe(Intl.getCanonicalLocales.length, 1);

// Returns Locales
shouldBe(Intl.getCanonicalLocales() instanceof Array, true);
// Doesn't care about `this`.
shouldBe(JSON.stringify(Intl.getCanonicalLocales.call(null, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.getCanonicalLocales.call({}, 'en')), '["en"]');
shouldBe(JSON.stringify(Intl.getCanonicalLocales.call(1, 'en')), '["en"]');
// Ignores non-object, non-string list.
shouldBe(JSON.stringify(Intl.getCanonicalLocales(9)), '[]');
// Makes an array of tags.
shouldBe(JSON.stringify(Intl.getCanonicalLocales('en')), '["en"]');
// Handles array-like objects with holes.
shouldBe(JSON.stringify(Intl.getCanonicalLocales({ length: 4, 1: 'en', 0: 'es', 3: 'de' })), '["es","en","de"]');
// Deduplicates tags.
shouldBe(JSON.stringify(Intl.getCanonicalLocales([ 'en', 'pt', 'en', 'es' ])), '["en","pt","es"]');
// Canonicalizes tags.
shouldBe(
    JSON.stringify(Intl.getCanonicalLocales('En-laTn-us-variAnt-fOObar-1abc-U-kn-tRue-A-aa-aaa-x-RESERVED')),
    $vm.icuVersion() >= 67
        ? '["en-Latn-US-1abc-foobar-variant-a-aa-aaa-u-kn-x-reserved"]'
        : '["en-Latn-US-variant-foobar-1abc-a-aa-aaa-u-kn-true-x-reserved"]'
);
// Replaces outdated tags.
shouldBe(JSON.stringify(Intl.getCanonicalLocales('no-bok')), '["nb"]');
// Canonicalizes private tags.
shouldBe(JSON.stringify(Intl.getCanonicalLocales('X-some-thing')), '["x-some-thing"]');
// Throws on problems with length, get, or toString.
shouldThrow(() => Intl.getCanonicalLocales(Object.create(null, { length: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.getCanonicalLocales(Object.create(null, { length: { value: 1 }, 0: { get() { throw new Error(); } } })), Error);
shouldThrow(() => Intl.getCanonicalLocales([ { toString() { throw new Error(); } } ]), Error);
// Throws on bad tags.
shouldThrow(() => Intl.getCanonicalLocales([ 5 ]), TypeError);
shouldThrow(() => Intl.getCanonicalLocales(''), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('a'), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('abcdefghij'), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('#$'), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('en-@-abc'), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('en-u'), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('en-u-kn-true-u-ko-true'), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('en-x'), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('en-*'), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('en-'), RangeError);
shouldThrow(() => Intl.getCanonicalLocales('en--US'), RangeError);
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
    shouldNotThrow(() => Intl.getCanonicalLocales(validLanguageTag));
