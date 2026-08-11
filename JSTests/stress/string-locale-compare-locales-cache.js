function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`bad error: ${String(error)}`);
}

const words = ["a", "A", "ä", "Z", "z", "ss", "ß", "ch", "cz", "資料", "10", "9", "co-op", "coop", ""];
const locales = ["en", "de", "sv", "ja", "es", "tr", "de-u-co-phonebk", "en-US", "und"];

// Repeated calls with the same string locale must keep matching a fresh Intl.Collator.
for (const locale of locales) {
    const collator = new Intl.Collator(locale);
    for (let i = 0; i < 3; ++i) {
        for (const x of words) {
            for (const y of words)
                shouldBe(x.localeCompare(y, locale), collator.compare(x, y));
        }
    }
}

// Alternating locales.
for (let i = 0; i < 100; ++i) {
    const locale = locales[i % locales.length];
    shouldBe("ä".localeCompare("z", locale), new Intl.Collator(locale).compare("ä", "z"));
}

// An invalid locale must throw on every call.
for (let i = 0; i < 3; ++i)
    shouldThrow(() => "a".localeCompare("b", "xx-fake-INVALID!"), RangeError);
shouldBe("a".localeCompare("b", "en"), -1);

// Non-string locales and explicit options take the general path.
shouldBe("a".localeCompare("b", ["en"]), -1);
shouldBe("a".localeCompare("b", undefined), -1);
shouldBe("a".localeCompare("A", "en", { sensitivity: "base" }), 0);
shouldBe("a".localeCompare("A", "en") !== 0, true);

// Rope locale string.
const prefix = "e";
shouldBe("a".localeCompare("b", prefix + "n"), -1);
