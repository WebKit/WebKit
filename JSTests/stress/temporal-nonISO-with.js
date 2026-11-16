//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

// Buddhist: batched fields.year must be BE year (not Gregorian), else with() double-shifts.
{
    const d = Temporal.PlainDate.from({ year: 2543, monthCode: "M01", day: 1, calendar: "buddhist" });
    shouldBe(d.year, 2543, "buddhist year");
    const withDay = d.with({ day: 15 });
    shouldBe(withDay.year, 2543, "buddhist with day preserves year");
    shouldBe(withDay.eraYear, 2543, "buddhist with day preserves eraYear");
    shouldBe(withDay.day, 15, "buddhist with day");
}

// ROC: with should preserve calendar year.
{
    const d = Temporal.PlainDate.from({ year: 113, monthCode: "M06", day: 15, calendar: "roc" });
    shouldBe(d.year, 113, "roc year");
    const withMonth = d.with({ month: 1 });
    shouldBe(withMonth.year, 113, "roc with month preserves year");
    shouldBe(withMonth.month, 1, "roc with month");
}

// Coptic: with a different day preserves M13 (13-month calendar).
{
    const d = Temporal.PlainDate.from({ year: 1741, monthCode: "M13", day: 3, calendar: "coptic" });
    shouldBe(d.month, 13, "coptic M13");
    const withDay = d.with({ day: 5 });
    shouldBe(withDay.year, 1741, "coptic with day preserves year");
    shouldBe(withDay.month, 13, "coptic with day preserves M13");
    shouldBe(withDay.day, 5, "coptic with day");
}

// Islamic-civil: with year advances era correctly.
{
    const d = Temporal.PlainDate.from({ year: 1445, monthCode: "M06", day: 15, calendar: "islamic-civil" });
    const withYear = d.with({ year: 1446 });
    shouldBe(withYear.year, 1446, "islamic-civil with year");
    shouldBe(withYear.month, 6, "islamic-civil with year preserves month");
    shouldBe(withYear.day, 15, "islamic-civil with year preserves day");
    shouldBe(withYear.era, "ah", "islamic-civil era");
    shouldBe(withYear.eraYear, 1446, "islamic-civil eraYear");
}
