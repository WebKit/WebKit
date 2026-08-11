//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${expected}, got ${actual}`);
}

// Buddhist: BE = ISO + 543, both directions across the representable range.
{
    // Minimum ISO year -271821 → BE -271278.
    const min = Temporal.PlainDate.from({ calendar: "buddhist", year: -271278, era: "be", eraYear: -271278, month: 4, monthCode: "M04", day: 19 });
    shouldBe(min.year, -271278, "buddhist min year");
    shouldBe(min.eraYear, -271278, "buddhist min eraYear");
    shouldBe(min.era, "be", "buddhist min era");
    // Maximum ISO year 275760 → BE 276303.
    const max = Temporal.PlainDate.from({ calendar: "buddhist", year: 276303, era: "be", eraYear: 276303, month: 9, monthCode: "M09", day: 13 });
    shouldBe(max.year, 276303, "buddhist max year");
    shouldBe(max.eraYear, 276303, "buddhist max eraYear");
}

// ROC: ISO 1912 = ROC 1; era boundary at ISO 1912.
{
    const min = Temporal.PlainDate.from({ calendar: "roc", year: -273732, era: "broc", eraYear: 273733, month: 4, monthCode: "M04", day: 19 });
    shouldBe(min.year, -273732, "roc min year");
    shouldBe(min.eraYear, 273733, "roc min eraYear");
    shouldBe(min.era, "broc", "roc min era");
    const max = Temporal.PlainDate.from({ calendar: "roc", year: 273849, era: "roc", eraYear: 273849, month: 9, monthCode: "M09", day: 13 });
    shouldBe(max.year, 273849, "roc max year");
    shouldBe(max.eraYear, 273849, "roc max eraYear");
    shouldBe(max.era, "roc", "roc max era");
}

// Japanese: pre-meiji reports ce/bce; ISO year direct.
{
    const bceExtreme = Temporal.PlainDate.from({ calendar: "japanese", year: -271821, era: "bce", eraYear: 271822, month: 4, monthCode: "M04", day: 19 });
    shouldBe(bceExtreme.year, -271821, "japanese bce extreme year");
    shouldBe(bceExtreme.eraYear, 271822, "japanese bce extreme eraYear");
    shouldBe(bceExtreme.era, "bce", "japanese bce extreme era");
}
