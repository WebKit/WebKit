// Verify that case-insensitive Unicode-mode character class ranges include the
// non-ASCII canonical equivalents U+017F (long s) and U+212A (Kelvin sign).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

const longS = "\u017F"; // LATIN SMALL LETTER LONG S, canonicalizes to "s".
const kelvin = "\u212A"; // KELVIN SIGN, canonicalizes to "k".

shouldBe(/[a-z]/iu.test(longS), true);
shouldBe(/[A-Z]/iu.test(longS), true);
shouldBe(/[s]/iu.test(longS), true);
shouldBe(/[r-t]/iu.test(longS), true);
shouldBe(/[a-z]/iv.test(longS), true);
shouldBe(/[a-r]/iu.test(longS), false);
shouldBe(/[t-z]/iu.test(longS), false);

shouldBe(/[a-z]/iu.test(kelvin), true);
shouldBe(/[A-Z]/iu.test(kelvin), true);
shouldBe(/[k]/iu.test(kelvin), true);
shouldBe(/[j-l]/iu.test(kelvin), true);
shouldBe(/[a-z]/iv.test(kelvin), true);
shouldBe(/[a-j]/iu.test(kelvin), false);
shouldBe(/[l-z]/iu.test(kelvin), false);

// Negated classes must exclude the canonical equivalents.
shouldBe(/[^a-z]/iu.test(longS), false);
shouldBe(/[^a-z]/iu.test(kelvin), false);

// Ranges crossing the ASCII boundary.
shouldBe(/[a-\u00FF]/iu.test(longS), true);
shouldBe(/[a-\u00FF]/iu.test(kelvin), true);

// Reverse direction: non-ASCII ranges containing the canonical equivalents
// must keep matching the ASCII characters.
shouldBe(/[\u0170-\u0180]/iu.test("s"), true);
shouldBe(/[\u0170-\u0180]/iu.test("S"), true);
shouldBe(/[\u2120-\u2130]/iu.test("k"), true);
shouldBe(/[\u2120-\u2130]/iu.test("K"), true);

// Without the i flag, no canonicalization.
shouldBe(/[a-z]/u.test(longS), false);
shouldBe(/[a-z]/u.test(kelvin), false);

// Non-Unicode mode (UCS2 canonicalization) must not change: U+017F/U+212A
// do not canonicalize into ASCII there.
shouldBe(/[a-z]/i.test(longS), false);
shouldBe(/[a-z]/i.test(kelvin), false);
