function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
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

shouldBe(String.prototype.toLocaleLowerCase.length, 0);

// Check empty string optimization.
shouldBe(''.toLocaleLowerCase(), '');

// Generic
shouldBe(String.prototype.toLocaleLowerCase.call({ toString() { return 'A' } }), 'a');
shouldThrow(() => String.prototype.toLocaleLowerCase.call({ toString() { throw new Error() } }), Error);
shouldThrow(() => String.prototype.toLocaleLowerCase.call(null), TypeError);
shouldThrow(() => String.prototype.toLocaleLowerCase.call(undefined), TypeError);

// Ignores non-object, non-string locale list.
shouldBe('A'.toLocaleLowerCase(9), 'a');
// Handles array-like objects with holes.
shouldBe('\u0130'.toLocaleLowerCase({ length: 4, 1: 'az', 3: 'en' }), 'i');
// Throws on problems with length, get, or toString.
shouldThrow(() => 'A'.toLocaleLowerCase(Object.create(null, { length: { get() { throw new Error() } } })), Error);
shouldThrow(() => 'A'.toLocaleLowerCase(Object.create(null, { length: { value: 1 }, 0: { get() { throw new Error() } } })), Error);
shouldThrow(() => 'A'.toLocaleLowerCase([ { toString() { throw new Error() } } ]), Error);
// Throws on bad tags.
shouldThrow(() => 'A'.toLocaleLowerCase([ 5 ]), TypeError);
shouldThrow(() => 'A'.toLocaleLowerCase(''), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('a'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('abcdefghij'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('#$'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('en-@-abc'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('en-u'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('en-u-kn-true-u-ko-true'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('en-x'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('en-*'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('en-'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('en--US'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('no-bok'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('x-some-thing'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('i-klingon'), RangeError); // grandfathered tag is not accepted by IsStructurallyValidLanguageTag
shouldThrow(() => 'A'.toLocaleLowerCase('x-en-US-12345'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('x-12345-12345-en-US'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('x-en-US-12345-12345'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('x-en-u-foo'), RangeError);
shouldThrow(() => 'A'.toLocaleLowerCase('x-en-u-foo-u-bar'), RangeError);

// Check ascii and accents.
shouldBe('AbCdEfGhIjKlMnOpQRStuvWXyZ0123456789'.toLocaleLowerCase(), 'abcdefghijklmnopqrstuvwxyz0123456789');
shouldBe('ÀÉÎÖŒØŪÑ'.toLocaleLowerCase(), 'àéîöœøūñ');

// Check non-special case for dotted I.
shouldBe('\u0130'.toLocaleLowerCase('und'), '\u0069\u0307');

// Check for special casing of Azeri.
shouldBe('\u0130'.toLocaleLowerCase('az'), 'i');
shouldBe('I\u0307'.toLocaleLowerCase('az'), 'i');
shouldBe('I\u0323\u0307'.toLocaleLowerCase('az'), 'i\u0323');
shouldBe('I\uD800\uDDFD\u0307'.toLocaleLowerCase('az'), 'i\uD800\uDDFD');
shouldBe('IA\u0307'.toLocaleLowerCase('az'), '\u0131a\u0307');
shouldBe('I\u0300\u0307'.toLocaleLowerCase('az'), '\u0131\u0300\u0307');
shouldBe('I\uD834\uDD85\u0307'.toLocaleLowerCase('az'), '\u0131\uD834\uDD85\u0307');
shouldBe('I'.toLocaleLowerCase('az'), '\u0131');
shouldBe('i'.toLocaleLowerCase('az'), 'i');
shouldBe('\u0131'.toLocaleLowerCase('az'), '\u0131');

// Check for special casing of Lithuanian.
shouldBe('I\u0300'.toLocaleLowerCase('lt'), 'i\u0307\u0300');
shouldBe('J\u0300'.toLocaleLowerCase('lt'), 'j\u0307\u0300');
shouldBe('\u012E\u0300'.toLocaleLowerCase('lt'), '\u012F\u0307\u0300');
shouldBe('I\uD834\uDD85'.toLocaleLowerCase('lt'), 'i\u0307\uD834\uDD85');
shouldBe('J\uD834\uDD85'.toLocaleLowerCase('lt'), 'j\u0307\uD834\uDD85');
shouldBe('\u012E\uD834\uDD85'.toLocaleLowerCase('lt'), '\u012F\u0307\uD834\uDD85');
shouldBe('I\u0325\u0300'.toLocaleLowerCase('lt'), 'i\u0307\u0325\u0300');
shouldBe('J\u0325\u0300'.toLocaleLowerCase('lt'), 'j\u0307\u0325\u0300');
shouldBe('\u012E\u0325\u0300'.toLocaleLowerCase('lt'), '\u012F\u0307\u0325\u0300');
shouldBe('I\uD800\uDDFD\u0300'.toLocaleLowerCase('lt'), 'i\u0307\uD800\uDDFD\u0300');
shouldBe('J\uD800\uDDFD\u0300'.toLocaleLowerCase('lt'), 'j\u0307\uD800\uDDFD\u0300');
shouldBe('\u012E\uD800\uDDFD\u0300'.toLocaleLowerCase('lt'), '\u012F\u0307\uD800\uDDFD\u0300');
shouldBe('I\u0325\uD834\uDD85'.toLocaleLowerCase('lt'), 'i\u0307\u0325\uD834\uDD85');
shouldBe('J\u0325\uD834\uDD85'.toLocaleLowerCase('lt'), 'j\u0307\u0325\uD834\uDD85');
shouldBe('\u012E\u0325\uD834\uDD85'.toLocaleLowerCase('lt'), '\u012F\u0307\u0325\uD834\uDD85');
shouldBe('I\uD800\uDDFD\uD834\uDD85'.toLocaleLowerCase('lt'), 'i\u0307\uD800\uDDFD\uD834\uDD85');
shouldBe('J\uD800\uDDFD\uD834\uDD85'.toLocaleLowerCase('lt'), 'j\u0307\uD800\uDDFD\uD834\uDD85');
shouldBe('\u012E\uD800\uDDFD\uD834\uDD85'.toLocaleLowerCase('lt'), '\u012F\u0307\uD800\uDDFD\uD834\uDD85');
shouldBe('IA\u0300'.toLocaleLowerCase('lt'), 'ia\u0300');
shouldBe('JA\u0300'.toLocaleLowerCase('lt'), 'ja\u0300');
shouldBe('\u012EA\u0300'.toLocaleLowerCase('lt'), '\u012Fa\u0300');
shouldBe('IA\uD834\uDD85'.toLocaleLowerCase('lt'), 'ia\uD834\uDD85');
shouldBe('JA\uD834\uDD85'.toLocaleLowerCase('lt'), 'ja\uD834\uDD85');
shouldBe('\u012EA\uD834\uDD85'.toLocaleLowerCase('lt'), '\u012Fa\uD834\uDD85');
shouldBe('\u00CC'.toLocaleLowerCase('lt'), '\u0069\u0307\u0300');
shouldBe('\u00CD'.toLocaleLowerCase('lt'), '\u0069\u0307\u0301');
shouldBe('\u0128'.toLocaleLowerCase('lt'), '\u0069\u0307\u0303');

// Check for special casing of Turkish.
shouldBe('\u0130'.toLocaleLowerCase('tr'), 'i');
shouldBe('I\u0307'.toLocaleLowerCase('tr'), 'i');
shouldBe('I\u0323\u0307'.toLocaleLowerCase('tr'), 'i\u0323');
shouldBe('I\uD800\uDDFD\u0307'.toLocaleLowerCase('tr'), 'i\uD800\uDDFD');
shouldBe('IA\u0307'.toLocaleLowerCase('tr'), '\u0131a\u0307');
shouldBe('I\u0300\u0307'.toLocaleLowerCase('tr'), '\u0131\u0300\u0307');
shouldBe('I\uD834\uDD85\u0307'.toLocaleLowerCase('tr'), '\u0131\uD834\uDD85\u0307');
shouldBe('I'.toLocaleLowerCase('tr'), '\u0131');
shouldBe('i'.toLocaleLowerCase('tr'), 'i');
shouldBe('\u0131'.toLocaleLowerCase('tr'), '\u0131');
