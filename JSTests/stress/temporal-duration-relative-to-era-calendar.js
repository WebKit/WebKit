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

// relativeTo PlainDate with era — era+eraYear resolve to correct ISO year
{
    const d = Temporal.Duration.from({ days: 10 });
    const r = d.total({ unit: "days", relativeTo: { era: "showa", eraYear: 50, month: 1, day: 1, calendar: "japanese" } });
    shouldBe(r, 10);
}

// relativeTo PlainDate era+eraYear: the resulting plain date has the right year
{
    const d = Temporal.Duration.from({ months: 1 });
    // Showa 50 = 1975 — adding 1 month to 1975-01-01 should give 1975-02-01
    const r = d.total({ unit: "months", relativeTo: { era: "showa", eraYear: 50, month: 1, day: 1, calendar: "japanese" } });
    shouldBe(r, 1);
}

// relativeTo ZDT with era
{
    const d = Temporal.Duration.from({ days: 10 });
    const r = d.total({ unit: "days", relativeTo: { era: "showa", eraYear: 50, month: 1, day: 1, calendar: "japanese", timeZone: "UTC" } });
    shouldBe(r, 10);
}

// relativeTo era+eraYear+year consistent → OK
{
    const d = Temporal.Duration.from({ days: 1 });
    const r = d.total({ unit: "days", relativeTo: { era: "showa", eraYear: 50, year: 1975, month: 1, day: 1, calendar: "japanese" } });
    shouldBe(r, 1);
}

// relativeTo era+eraYear+year inconsistent → RangeError
{
    shouldThrow(() => {
        const d = Temporal.Duration.from({ days: 1 });
        d.total({ unit: "days", relativeTo: { era: "showa", eraYear: 50, year: 2000, month: 1, day: 1, calendar: "japanese" } });
    }, RangeError);
}

// iso8601 relativeTo unchanged
{
    const d = Temporal.Duration.from({ days: 5 });
    const r = d.total({ unit: "days", relativeTo: { year: 2024, month: 1, day: 1 } });
    shouldBe(r, 5);
}
