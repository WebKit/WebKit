function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected}, but got ${actual}`);
}

const locales = ["en", "fr", "pl", "zh"];
const numbers = [0, 1, 2, 3, 4, 5, 10, 100, 1e3, 1e4, 1e5, 1e6];

function representations(number) {
    return [
        `${number}`,
        `0b${number.toString(2)}`,
        `0B${number.toString(2)}`,
        `0o${number.toString(8)}`,
        `0O${number.toString(8)}`,
        `0x${number.toString(16)}`,
        `0X${number.toString(16)}`,
    ];
}

const whitespaceCharacters = [
    "\u0009", "\u000B", "\u000C", "\uFEFF", "\u0020", "\u00A0", "\u1680",
    "\u2000", "\u2001", "\u2002", "\u2003", "\u2004", "\u2005", "\u2006",
    "\u2007", "\u2008", "\u2009", "\u200A", "\u202F", "\u205F", "\u3000",
    "\u000A", "\u000D", "\u2028", "\u2029",
];

for (const locale of locales) {
    const pluralRules = new Intl.PluralRules(locale);

    for (const number of numbers) {
        const expected = pluralRules.select(number);

        shouldBe(pluralRules.select(BigInt(number)), expected, `${locale}: BigInt ${number}`);

        for (const representation of representations(number)) {
            shouldBe(pluralRules.select(representation), expected, `${locale}: string ${representation}`);

            for (const whitespace of whitespaceCharacters) {
                const codePoint = whitespace.codePointAt(0).toString(16).padStart(4, "0");
                shouldBe(pluralRules.select(whitespace + representation), expected, `${locale}: leading U+${codePoint} and ${representation}`);
                shouldBe(pluralRules.select(representation + whitespace), expected, `${locale}: trailing U+${codePoint} and ${representation}`);
                shouldBe(pluralRules.select(whitespace + representation + whitespace), expected, `${locale}: surrounding U+${codePoint} and ${representation}`);
            }
        }
    }

    if (Intl.PluralRules.prototype.selectRange) {
        for (const start of numbers) {
            for (const end of numbers) {
                const expected = pluralRules.selectRange(start, end);

                shouldBe(pluralRules.selectRange(BigInt(start), BigInt(end)), expected, `${locale}: BigInt range ${start}-${end}`);
                shouldBe(pluralRules.selectRange(start, BigInt(end)), expected, `${locale}: Number/BigInt range ${start}-${end}`);
                shouldBe(pluralRules.selectRange(BigInt(start), end), expected, `${locale}: BigInt/Number range ${start}-${end}`);

                for (const representation of representations(start))
                    shouldBe(pluralRules.selectRange(representation, String(end)), expected, `${locale}: string range ${representation}-${end}`);
            }
        }
    }
}

const ceilPluralRules = new Intl.PluralRules("en", {
    roundingMode: "ceil",
    maximumFractionDigits: 0,
});
shouldBe(ceilPluralRules.select(1.0000000000000001), ceilPluralRules.select(1), "Number input is rounded before selection");
shouldBe(ceilPluralRules.select("1.0000000000000001"), ceilPluralRules.select(2), "precise string input rounds toward positive infinity");

const floorPluralRules = new Intl.PluralRules("en", {
    roundingMode: "floor",
    maximumFractionDigits: 0,
});
shouldBe(floorPluralRules.select(0.99999999999999999), floorPluralRules.select(1), "Number input is rounded before selection");
shouldBe(floorPluralRules.select("0.99999999999999999"), floorPluralRules.select(0), "precise string input rounds toward negative infinity");
