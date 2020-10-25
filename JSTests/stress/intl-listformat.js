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

function test() {
    {
        // https://tc39.github.io/ecma402/#pluralrules-objects
        // The Intl.ListFormat Constructor

        // The ListFormat constructor is the %ListFormat% intrinsic object and a standard built-in property of the Intl object.
        shouldBe(Intl.ListFormat instanceof Function, true);

        // Intl.ListFormat ([ locales [, options ] ])

        // If NewTarget is undefined, throw a TypeError exception.
        shouldThrow(() => Intl.ListFormat(), TypeError);
        shouldThrow(() => Intl.ListFormat.call({}), TypeError);

        shouldThrow(() => new Intl.ListFormat('$'), RangeError);
        shouldThrow(() => new Intl.ListFormat('en', null), TypeError);
        shouldBe(new Intl.ListFormat() instanceof Intl.ListFormat, true);

        // Subclassable
        {
            class DerivedListFormat extends Intl.ListFormat {};
            shouldBe((new DerivedListFormat) instanceof DerivedListFormat, true);
            shouldBe((new DerivedListFormat) instanceof Intl.ListFormat, true);
            shouldBe(new DerivedListFormat('en').format(['Orange', 'Apple', 'Lemon']), 'Orange, Apple, and Lemon');
            shouldBe(Object.getPrototypeOf(new DerivedListFormat), DerivedListFormat.prototype);
            shouldBe(Object.getPrototypeOf(Object.getPrototypeOf(new DerivedListFormat)), Intl.ListFormat.prototype);
        }

        // Properties of the Intl.ListFormat Constructor

        // length property (whose value is 0)
        shouldBe(Intl.ListFormat.length, 0);

        // Intl.ListFormat.prototype

        // This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
        shouldBe(Object.getOwnPropertyDescriptor(Intl.ListFormat, 'prototype').writable, false);
        shouldBe(Object.getOwnPropertyDescriptor(Intl.ListFormat, 'prototype').enumerable, false);
        shouldBe(Object.getOwnPropertyDescriptor(Intl.ListFormat, 'prototype').configurable, false);

        // Intl.ListFormat.supportedLocalesOf (locales [, options ])

        // The value of the length property of the supportedLocalesOf method is 1.
        shouldBe(Intl.ListFormat.supportedLocalesOf.length, 1);

        // Returns SupportedLocales
        shouldBe(Intl.ListFormat.supportedLocalesOf() instanceof Array, true);
        // Doesn't care about `this`.
        shouldBe(JSON.stringify(Intl.ListFormat.supportedLocalesOf.call(null, 'en')), '["en"]');
        shouldBe(JSON.stringify(Intl.ListFormat.supportedLocalesOf.call({}, 'en')), '["en"]');
        shouldBe(JSON.stringify(Intl.ListFormat.supportedLocalesOf.call(1, 'en')), '["en"]');
        // Ignores non-object, non-string list.
        shouldBe(JSON.stringify(Intl.ListFormat.supportedLocalesOf(9)), '[]');
        // Makes an array of tags.
        shouldBe(JSON.stringify(Intl.ListFormat.supportedLocalesOf('en')), '["en"]');
        // Handles array-like objects with holes.
        shouldBe(JSON.stringify(Intl.ListFormat.supportedLocalesOf({ length: 4, 1: 'en', 0: 'es', 3: 'de' })), '["es","en","de"]');
        // Deduplicates tags.
        shouldBe(JSON.stringify(Intl.ListFormat.supportedLocalesOf([ 'en', 'pt', 'en', 'es' ])), '["en","pt","es"]');
        // Canonicalizes tags.
        shouldBe(
            JSON.stringify(Intl.ListFormat.supportedLocalesOf('En-laTn-us-variAnt-fOObar-1abc-U-kn-tRue-A-aa-aaa-x-RESERVED')),
            $vm.icuVersion() >= 67
                ? '["en-Latn-US-1abc-foobar-variant-a-aa-aaa-u-kn-x-reserved"]'
                : '["en-Latn-US-variant-foobar-1abc-a-aa-aaa-u-kn-x-reserved"]'
        );
        // Throws on problems with length, get, or toString.
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf(Object.create(null, { length: { get() { throw new Error(); } } })), Error);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf(Object.create(null, { length: { value: 1 }, 0: { get() { throw new Error(); } } })), Error);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf([ { toString() { throw new Error(); } } ]), Error);
        // Throws on bad tags.
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('no-bok'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('x-some-thing'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf([ 5 ]), TypeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf(''), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('a'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('abcdefghij'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('#$'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('en-@-abc'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('en-u'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('en-u-kn-true-u-ko-true'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('en-x'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('en-*'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('en-'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('en--US'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('i-klingon'), RangeError); // grandfathered tag is not accepted by IsStructurallyValidLanguageTag
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('x-en-US-12345'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('x-12345-12345-en-US'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('x-en-US-12345-12345'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('x-en-u-foo'), RangeError);
        shouldThrow(() => Intl.ListFormat.supportedLocalesOf('x-en-u-foo-u-bar'), RangeError);

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
            shouldNotThrow(() => Intl.ListFormat.supportedLocalesOf(validLanguageTag));

        // Properties of the Intl.ListFormat Prototype Object

        // The Intl.ListFormat prototype object is itself an ordinary object.
        shouldBe(Object.getPrototypeOf(Intl.ListFormat.prototype), Object.prototype);

        // Intl.ListFormat.prototype.constructor
        // The initial value of Intl.ListFormat.prototype.constructor is the intrinsic object %ListFormat%.
        shouldBe(Intl.ListFormat.prototype.constructor, Intl.ListFormat);

        // Intl.ListFormat.prototype [ @@toStringTag ]
        // The initial value of the @@toStringTag property is the string value "Intl.ListFormat".
        shouldBe(Intl.ListFormat.prototype[Symbol.toStringTag], 'Intl.ListFormat');
        shouldBe(Object.prototype.toString.call(Intl.ListFormat.prototype), '[object Intl.ListFormat]');
        // This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
        shouldBe(Object.getOwnPropertyDescriptor(Intl.ListFormat.prototype, Symbol.toStringTag).writable, false);
        shouldBe(Object.getOwnPropertyDescriptor(Intl.ListFormat.prototype, Symbol.toStringTag).enumerable, false);
        shouldBe(Object.getOwnPropertyDescriptor(Intl.ListFormat.prototype, Symbol.toStringTag).configurable, true);
    }
    {
        const lf = new Intl.ListFormat("en", {
            localeMatcher: "best fit",
            type: "conjunction",
            style: "long",
        });
        shouldBe(lf.format(['Motorcycle', 'Truck' , 'Car']), `Motorcycle, Truck, and Car`);
        shouldBe(lf.format([]), ``);
        shouldBe(lf.format(), ``);
        shouldBe(lf.format(undefined), ``);
        shouldBe(lf.format("Apple"), `A, p, p, l, and e`);
        shouldThrow(() => lf.format(42), TypeError);
        shouldThrow(() => lf.format(null), TypeError);
        shouldThrow(() => lf.format([null]), TypeError);
        shouldBe(JSON.stringify(lf.resolvedOptions()), `{"locale":"en","type":"conjunction","style":"long"}`);
        shouldBe(JSON.stringify(Reflect.getOwnPropertyDescriptor(Intl.ListFormat.prototype, Symbol.toStringTag)), `{"value":"Intl.ListFormat","writable":false,"enumerable":false,"configurable":true}`);
    }
    {
        const list = ['Motorcycle', 'Bus', 'Car'];
        shouldBe(new Intl.ListFormat('en-GB', { style: 'long', type: 'conjunction' }).format(list), `Motorcycle, Bus and Car`);
        shouldBe(new Intl.ListFormat('en-GB', { style: 'short', type: 'conjunction' }).format(list), `Motorcycle, Bus and Car`);
        shouldBe(new Intl.ListFormat('en-GB', { style: 'narrow', type: 'conjunction' }).format(list), `Motorcycle, Bus, Car`);
        shouldBe(new Intl.ListFormat('en-GB', { style: 'long', type: 'disjunction' }).format(list), `Motorcycle, Bus or Car`);
        shouldBe(new Intl.ListFormat('en-GB', { style: 'short', type: 'disjunction' }).format(list), `Motorcycle, Bus or Car`);
        shouldBe(new Intl.ListFormat('en-GB', { style: 'narrow', type: 'disjunction' }).format(list), `Motorcycle, Bus or Car`);
        shouldBe(new Intl.ListFormat('en-GB', { style: 'long', type: 'unit' }).format(list), `Motorcycle, Bus, Car`);
        shouldBe(new Intl.ListFormat('en-GB', { style: 'short', type: 'unit' }).format(list), `Motorcycle, Bus, Car`);
        shouldBe(new Intl.ListFormat('en-GB', { style: 'narrow', type: 'unit' }).format(list), `Motorcycle Bus Car`);
    }
    {
        const list = ['バイク', 'バス', '車'];
        shouldBe(new Intl.ListFormat('ja-JP', { style: 'long', type: 'conjunction' }).format(list), `バイク、バス、車`);
        shouldBe(new Intl.ListFormat('ja-JP', { style: 'short', type: 'conjunction' }).format(list), `バイク、バス、車`);
        shouldBe(new Intl.ListFormat('ja-JP', { style: 'narrow', type: 'conjunction' }).format(list), `バイク、バス、車`);
        shouldBe(new Intl.ListFormat('ja-JP', { style: 'long', type: 'disjunction' }).format(list), `バイク、バス、または車`);
        shouldBe(new Intl.ListFormat('ja-JP', { style: 'short', type: 'disjunction' }).format(list), `バイク、バス、または車`);
        shouldBe(new Intl.ListFormat('ja-JP', { style: 'narrow', type: 'disjunction' }).format(list), `バイク、バス、または車`);
        shouldBe(new Intl.ListFormat('ja-JP', { style: 'long', type: 'unit' }).format(list), `バイク バス 車`);
        shouldBe(new Intl.ListFormat('ja-JP', { style: 'short', type: 'unit' }).format(list), `バイク バス 車`);
        shouldBe(new Intl.ListFormat('ja-JP', { style: 'narrow', type: 'unit' }).format(list), `バイクバス車`);
    }
    {
        const list = ['Motorcycle', 'Bus', 'Car'];
        const expected = [
            { "type": "element", "value": "Motorcycle" },
            { "type": "literal", "value": ", " },
            { "type": "element", "value": "Bus" },
            { "type": "literal", "value": " and " },
            { "type": "element", "value": "Car" }
        ];

        const lf = new Intl.ListFormat('en-GB', { style: 'long', type: 'conjunction' });
        const parts = lf.formatToParts(list);
        shouldBe(parts.length, expected.length);
        for (let i = 0; i < parts.length; ++i) {
            shouldBe(parts[i].type, expected[i].type);
            shouldBe(parts[i].value, expected[i].value);
        }

        shouldBe(JSON.stringify(lf.formatToParts([])), `[]`);
        shouldBe(JSON.stringify(lf.formatToParts()), `[]`);
        shouldBe(JSON.stringify(lf.formatToParts(undefined)), `[]`);
        shouldBe(JSON.stringify(lf.formatToParts("Apple")), `[{"type":"element","value":"A"},{"type":"literal","value":", "},{"type":"element","value":"p"},{"type":"literal","value":", "},{"type":"element","value":"p"},{"type":"literal","value":", "},{"type":"element","value":"l"},{"type":"literal","value":" and "},{"type":"element","value":"e"}]`);
        shouldThrow(() => lf.formatToParts(42), TypeError);
        shouldThrow(() => lf.formatToParts(null), TypeError);
        shouldThrow(() => lf.formatToParts([null]), TypeError);
    }
}

if (typeof Intl.ListFormat !== 'undefined')
    test();
