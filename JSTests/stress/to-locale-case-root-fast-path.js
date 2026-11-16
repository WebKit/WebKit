function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

const inputs = [
    "",
    "Hello World",
    "HELLO WORLD",
    "hello world",
    "ÀÉÎ",
    "àéî",
    "ß",
    "ΟΔΟΣ",
    "Größe",
    "\u{10400}",   // Deseret capital (non-BMP)
    "\u{10428}",   // Deseret small (non-BMP)
    "ABCdef123",
    "Ì",
];

// Core invariant of the optimization: for any non-tailoring locale, toLocaleLowerCase /
// toLocaleUpperCase must produce exactly the locale-independent result.
for (const s of inputs) {
    const rootLower = s.toLowerCase();
    const rootUpper = s.toUpperCase();
    for (const locale of [undefined, "en", "de", "ja", "fr", "en-US", "zh-Hans", "ru", "es-419"]) {
        shouldBe(s.toLocaleLowerCase(locale), rootLower);
        shouldBe(s.toLocaleUpperCase(locale), rootUpper);
    }
}

// Identity preservation: an already-lowercased/uppercased string round-trips unchanged.
shouldBe("hello world".toLocaleLowerCase(), "hello world");
shouldBe("HELLO WORLD".toLocaleUpperCase(), "HELLO WORLD");
shouldBe("".toLocaleLowerCase("en"), "");
shouldBe("".toLocaleUpperCase("en"), "");

// German sharp-s uppercases to "SS" in the root locale.
shouldBe("ß".toLocaleUpperCase(), "SS");

// Tailoring locales must keep going through ICU and differ from the root mapping.

// Turkish / Azeri: dotted/dotless i.
shouldBe("I".toLocaleLowerCase("tr"), "ı");
shouldBe("i".toLocaleUpperCase("tr"), "İ");
shouldBe("I".toLocaleLowerCase("az"), "ı");
shouldBe("i".toLocaleUpperCase("az"), "İ");

// Unicode extension subtags must not defeat tailoring detection.
shouldBe("I".toLocaleLowerCase("tr-u-co-phonebk"), "ı");

// Greek: uppercasing strips the tonos accent, differing from the root mapping.
if ("Άυτη".toLocaleUpperCase("el") === "Άυτη".toUpperCase())
    throw new Error("Greek uppercasing should differ from root");

// Lithuanian: combining dot above is retained on lowercase of accented I, so it
// differs from the root locale-independent lowercasing.
if ("Ì".toLocaleLowerCase("lt") === "Ì".toLowerCase())
    throw new Error("Lithuanian lowercasing should differ from root");

// Empty string with an explicit tailoring locale.
shouldBe("".toLocaleLowerCase("tr"), "");
shouldBe("".toLocaleUpperCase("el"), "");
