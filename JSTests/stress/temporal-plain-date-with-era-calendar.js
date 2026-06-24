//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

function shouldThrow(func, errorType) {
    let threw = false;
    try { func(); } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error(`Expected ${errorType.name} but got ${e.constructor.name}: ${e.message}`);
    }
    if (!threw)
        throw new Error(`Expected ${errorType.name} but no exception was thrown`);
}

// PlainDate.with() era+eraYear — Showa 50 = 1975
{
    const pd = Temporal.PlainDate.from({ era: "showa", eraYear: 45, month: 1, day: 1, calendar: "japanese" });
    shouldBe(pd.era, "showa");
    shouldBe(pd.eraYear, 45);
    const r = pd.with({ era: "showa", eraYear: 50 });
    shouldBe(r.year, 1975);
    shouldBe(r.era, "showa");
    shouldBe(r.eraYear, 50);
    shouldBe(r.month, 1);
    shouldBe(r.day, 1);
}

// Switch era: Heisei 5 = 1993
{
    const pd = Temporal.PlainDate.from({ era: "showa", eraYear: 45, month: 6, day: 15, calendar: "japanese" });
    const r = pd.with({ era: "heisei", eraYear: 5 });
    shouldBe(r.year, 1993);
    shouldBe(r.era, "heisei");
    shouldBe(r.eraYear, 5);
    shouldBe(r.month, 6);
    shouldBe(r.day, 15);
}

// era+eraYear+year consistent → OK
{
    const pd = Temporal.PlainDate.from({ era: "showa", eraYear: 45, month: 1, day: 1, calendar: "japanese" });
    const r = pd.with({ era: "showa", eraYear: 50, year: 1975 });
    shouldBe(r.year, 1975);
    shouldBe(r.eraYear, 50);
}

// era+eraYear+year inconsistent → RangeError
{
    const pd = Temporal.PlainDate.from({ era: "showa", eraYear: 45, month: 1, day: 1, calendar: "japanese" });
    shouldThrow(() => pd.with({ era: "showa", eraYear: 50, year: 2000 }), RangeError);
}

// era only (no eraYear) → TypeError
{
    const pd = Temporal.PlainDate.from({ era: "showa", eraYear: 45, month: 1, day: 1, calendar: "japanese" });
    shouldThrow(() => pd.with({ era: "showa" }), TypeError);
}

// eraYear only (no era) → TypeError
{
    const pd = Temporal.PlainDate.from({ era: "showa", eraYear: 45, month: 1, day: 1, calendar: "japanese" });
    shouldThrow(() => pd.with({ eraYear: 50 }), TypeError);
}

// Changing only day (no era) — era inherited from base
{
    const pd = Temporal.PlainDate.from({ era: "showa", eraYear: 50, month: 6, day: 15, calendar: "japanese" });
    const r = pd.with({ day: 20 });
    shouldBe(r.year, 1975);
    shouldBe(r.era, "showa");
    shouldBe(r.eraYear, 50);
    shouldBe(r.day, 20);
}

// iso8601 path unchanged
{
    const pd = new Temporal.PlainDate(2020, 3, 14);
    const r = pd.with({ year: 2024 });
    shouldBe(r.year, 2024);
    shouldBe(r.month, 3);
    shouldBe(r.day, 14);
}

// Japanese: changing month/day re-derives era (eras don't align with year boundaries).
// Showa 64 Jan 7 = 1989-01-07. Heisei started Jan 8 1989.
// Changing month to June: era re-derives to heisei (not inherited showa).
{
    const pd = Temporal.PlainDate.from({ era: "showa", eraYear: 64, month: 1, day: 7, calendar: "japanese" });
    shouldBe(pd.era, "showa");
    shouldBe(pd.eraYear, 64);
    const r = pd.with({ month: 6 });
    shouldBe(r.year, 1989);
    shouldBe(r.era, "heisei");
    shouldBe(r.eraYear, 1);
    shouldBe(r.month, 6);
}
