//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${expected}, got ${actual}`);
}

function shouldThrow(fn, label) {
    let threw = false;
    try { fn(); }
    catch (e) { threw = e instanceof RangeError; }
    shouldBe(threw, true, label);
}

// gregory/japanese: ad -> ce, bc -> bce (case-insensitive).
{
    const ad = Temporal.PlainDate.from({ calendar: "gregory", era: "ad", eraYear: 2024, year: 2024, month: 1, day: 1 });
    shouldBe(ad.era, "ce", "gregory ad -> ce");
    shouldBe(ad.eraYear, 2024, "gregory ad eraYear");
    const bc = Temporal.PlainDate.from({ calendar: "gregory", era: "bc", eraYear: 44, year: -43, month: 3, day: 15 });
    shouldBe(bc.era, "bce", "gregory bc -> bce");
    shouldBe(bc.eraYear, 44, "gregory bc eraYear");
    const jad = Temporal.PlainDate.from({ calendar: "japanese", era: "ad", eraYear: 1, month: 1, day: 1 });
    shouldBe(jad.era, "ce", "japanese ad -> ce");
    const upper = Temporal.PlainDate.from({ calendar: "gregory", era: "AD", eraYear: 1, year: 1, month: 1, day: 1 });
    shouldBe(upper.era, "ce", "AD (upper) -> ce");
}

// Non-alias calendars still reject "ad"/"bc".
for (const cal of ["ethiopic", "buddhist", "hebrew", "islamic-civil", "persian", "roc"]) {
    shouldThrow(() => Temporal.PlainDate.from({ calendar: cal, era: "ad", eraYear: 1, month: 1, day: 1 }), `${cal} rejects ad`);
    shouldThrow(() => Temporal.PlainDate.from({ calendar: cal, era: "bc", eraYear: 1, month: 1, day: 1 }), `${cal} rejects bc`);
}

// Canonical form still works.
{
    const ce = Temporal.PlainDate.from({ calendar: "gregory", era: "ce", eraYear: 2024, year: 2024, month: 1, day: 1 });
    shouldBe(ce.era, "ce", "gregory ce -> ce");
    const bce = Temporal.PlainDate.from({ calendar: "gregory", era: "bce", eraYear: 44, year: -43, month: 3, day: 15 });
    shouldBe(bce.era, "bce", "gregory bce -> bce");
}
