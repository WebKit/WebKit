function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function shouldThrow(func, expectedError) {
    let error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    
    if (!error)
        throw new Error('Expected function to throw');
    
    if (!(error instanceof expectedError))
        throw new Error(`Expected ${expectedError.name} but got ${error.constructor.name}`);
}

let descriptor = Object.getOwnPropertyDescriptor(Intl.Locale.prototype, 'variants');
shouldBe(typeof descriptor.get, 'function');
shouldBe(descriptor.set, undefined);
shouldBe(descriptor.enumerable, false);
shouldBe(descriptor.configurable, true);

{
    let locale = new Intl.Locale('en');
    shouldBe(locale.variants, undefined);
}
{
    let locale = new Intl.Locale('en-1996');
    shouldBe(locale.variants, '1996');
}
{
    let locale = new Intl.Locale('de-1901-emodeng');
    shouldBe(locale.variants, '1901-emodeng');
}

{
    let locale = new Intl.Locale('en-oxendict-spanglis');
    shouldBe(locale.variants, 'oxendict-spanglis');
}

{
    let locale = new Intl.Locale('de-latn-de-fonipa-1996-u-ca-gregory');
    shouldBe(locale.variants, '1996-fonipa');
}

// Test constructor options
{
    let locale = new Intl.Locale('en', { variants: 'emodeng' });
    shouldBe(locale.variants, 'emodeng');
}

{
    let locale = new Intl.Locale('en', { variants: 'spanglis-oxendict' });
    shouldBe(locale.variants, 'oxendict-spanglis');
}

{
    let locale = new Intl.Locale('en-1996', { variants: 'emodeng' });
    shouldBe(locale.variants, 'emodeng');
}

{
    let locale = new Intl.Locale('xx', { variants: '1xyz-1234-abcde-12345678' });
    shouldBe(locale.variants, '1234-12345678-1xyz-abcde');
}

{
    let locale = new Intl.Locale('ja', { variants: undefined });
    shouldBe(locale.variants, undefined);
}

{
    // valid variants
    shouldBe(new Intl.Locale('en', { variants: 'spanglis' }).variants, 'spanglis');
    shouldBe(new Intl.Locale('en', { variants: '1234' }).variants, '1234');
    shouldBe(new Intl.Locale('en', { variants: 'abcde' }).variants, 'abcde');
    shouldBe(new Intl.Locale('en', { variants: '12345678' }).variants, '12345678');

    // invalid variants
    shouldThrow(() => new Intl.Locale('en', { variants: '' }), RangeError);
    shouldThrow(() => new Intl.Locale('en', { variants: 'abc' }), RangeError);
    shouldThrow(() => new Intl.Locale('en', { variants: '123456789' }), RangeError);
    shouldThrow(() => new Intl.Locale('en', { variants: 'abc@' }), RangeError);
    shouldThrow(() => new Intl.Locale('en', { variants: 'abc-def' }), RangeError);
    shouldThrow(() => new Intl.Locale('en', { variants: 'fonipa-fonipa' }), RangeError);
    shouldThrow(() => new Intl.Locale('en', { variants: 'fonipa-valencia-Fonipa' }), RangeError);
    shouldThrow(() => new Intl.Locale('en', { variants: '-spanglis' }), RangeError);
    shouldThrow(() => new Intl.Locale('en', { variants: 'spanglis-' }), RangeError);
    shouldThrow(() => new Intl.Locale('en', { variants: 'spanglis--oxendict' }), RangeError);
}

{
    let locale = new Intl.Locale('en', { variants: 'spanglis-oxendict' });
    shouldBe(locale.toString(), 'en-oxendict-spanglis');
    shouldBe(locale.baseName, 'en-oxendict-spanglis');
    shouldBe(locale.variants, 'oxendict-spanglis'); // Alphabetical order
}

{
    let locale = new Intl.Locale('xx', { variants: '1xyz-1234-abcde-12345678' });
    shouldBe(locale.toString(), 'xx-1234-12345678-1xyz-abcde');
    shouldBe(locale.baseName, 'xx-1234-12345678-1xyz-abcde');
    shouldBe(locale.variants, '1234-12345678-1xyz-abcde'); // Canonical order
}

{
    let locale = new Intl.Locale('en-spanglis-oxendict');
    shouldBe(locale.variants, 'oxendict-spanglis');
}

{
    let locale = new Intl.Locale('en', { variants: 'EMODENG' });
    shouldBe(locale.variants, 'emodeng');
}

{
    let locale = new Intl.Locale('en', { variants: 'SpAnGlIs-OxEnDiCt' });
    shouldBe(locale.variants, 'oxendict-spanglis');
}

{
    let variantsGetter = Object.getOwnPropertyDescriptor(Intl.Locale.prototype, 'variants').get;

    shouldThrow(() => variantsGetter.call(undefined), TypeError);
    shouldThrow(() => variantsGetter.call(null), TypeError);
    shouldThrow(() => variantsGetter.call(true), TypeError);
    shouldThrow(() => variantsGetter.call('string'), TypeError);
    shouldThrow(() => variantsGetter.call(123), TypeError);
    shouldThrow(() => variantsGetter.call({}), TypeError);
    shouldThrow(() => variantsGetter.call([]), TypeError);
    shouldThrow(() => variantsGetter.call(Symbol()), TypeError);

    shouldThrow(() => variantsGetter.call(Intl.Locale.prototype), TypeError);
}

{
    let variantsGetter = Object.getOwnPropertyDescriptor(Intl.Locale.prototype, 'variants').get;
    let locale = new Intl.Locale('en-1996');
    shouldBe(variantsGetter.call(locale), '1996');
}
