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

// era+eraYear changes year (Showa 45 → Showa 50 = 1975), time preserved
{
    const pdt = Temporal.PlainDateTime.from({ era: "showa", eraYear: 45, month: 1, day: 1, hour: 12, calendar: "japanese" });
    const r = pdt.with({ era: "showa", eraYear: 50 });
    shouldBe(r.year, 1975);
    shouldBe(r.era, "showa");
    shouldBe(r.eraYear, 50);
    shouldBe(r.hour, 12);
}

// year-only: era re-derived from resolved date
{
    const pdt = Temporal.PlainDateTime.from({ era: "showa", eraYear: 45, month: 6, day: 15, hour: 10, minute: 30, calendar: "japanese" });
    const r = pdt.with({ year: 1975 });
    shouldBe(r.year, 1975);
    shouldBe(r.era, "showa");
    shouldBe(r.eraYear, 50);
    shouldBe(r.hour, 10);
    shouldBe(r.minute, 30);
}

// Japanese month change → era re-derived (Showa 64 Jan → month=6 → Heisei 1 Jun)
{
    const pdt = Temporal.PlainDateTime.from({ era: "showa", eraYear: 64, month: 1, day: 7, hour: 8, calendar: "japanese" });
    const r = pdt.with({ month: 6 });
    shouldBe(r.year, 1989);
    shouldBe(r.era, "heisei");
    shouldBe(r.eraYear, 1);
    shouldBe(r.month, 6);
    shouldBe(r.hour, 8);
}

// time-only change — date preserved including era
{
    const pdt = Temporal.PlainDateTime.from({ era: "showa", eraYear: 50, month: 6, day: 15, hour: 9, calendar: "japanese" });
    const r = pdt.with({ hour: 17, minute: 30 });
    shouldBe(r.year, 1975);
    shouldBe(r.era, "showa");
    shouldBe(r.eraYear, 50);
    shouldBe(r.hour, 17);
    shouldBe(r.minute, 30);
}

// era only (no eraYear) → TypeError
{
    const pdt = Temporal.PlainDateTime.from({ era: "showa", eraYear: 45, month: 1, day: 1, calendar: "japanese" });
    shouldThrow(() => pdt.with({ era: "showa" }), TypeError);
}

// era+eraYear+year inconsistent → RangeError
{
    const pdt = Temporal.PlainDateTime.from({ era: "showa", eraYear: 45, month: 1, day: 1, calendar: "japanese" });
    shouldThrow(() => pdt.with({ era: "showa", eraYear: 50, year: 2000 }), RangeError);
}

// iso8601 path unchanged
{
    const pdt = new Temporal.PlainDateTime(2024, 3, 14, 9, 30, 0);
    const r = pdt.with({ year: 2025, hour: 12 });
    shouldBe(r.year, 2025);
    shouldBe(r.month, 3);
    shouldBe(r.day, 14);
    shouldBe(r.hour, 12);
    shouldBe(r.minute, 30);
}
