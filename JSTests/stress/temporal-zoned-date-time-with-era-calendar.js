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

// --- Japanese calendar era tests ---

// Showa 50 = 1975: era+eraYear without explicit year
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "japanese");
    const result = zdt.with({ era: "showa", eraYear: 50, month: 1, day: 5 });
    shouldBe(result.year, 1975);
    shouldBe(result.month, 1);
    shouldBe(result.day, 5);
    shouldBe(result.era, "showa");
    shouldBe(result.eraYear, 50);
}

// Month inherited from base (epoch 0 = Jan), not explicit.
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "japanese");
    const result = zdt.with({ era: "showa", eraYear: 50, day: 5 });
    shouldBe(result.toString(), "1975-01-05T00:00:00+00:00[UTC][u-ca=japanese]");
    shouldBe(result.year, 1975);
    shouldBe(result.month, 1);
    shouldBe(result.day, 5);
}

// Heisei 1 = 1989: era+eraYear without explicit year
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "japanese");
    const result = zdt.with({ era: "heisei", eraYear: 1, month: 1, day: 8 });
    shouldBe(result.year, 1989);
    shouldBe(result.era, "heisei");
    shouldBe(result.eraYear, 1);
}

// era+eraYear+year all present: should all agree (Showa 50 = 1975)
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "japanese");
    const result = zdt.with({ era: "showa", eraYear: 50, year: 1975, month: 1, day: 5 });
    shouldBe(result.year, 1975);
    shouldBe(result.eraYear, 50);
}

// Change eraYear (with era) on an existing era-based ZDT: Showa 50->51 = 1975->1976
{
    const base = Temporal.ZonedDateTime.from("1975-06-15T12:00:00+00:00[UTC][u-ca=japanese]");
    shouldBe(base.era, "showa");
    shouldBe(base.eraYear, 50);
    const result = base.with({ era: "showa", eraYear: 51 });
    shouldBe(result.year, 1976);
    shouldBe(result.era, "showa");
    shouldBe(result.eraYear, 51);
    shouldBe(result.month, 6);
    shouldBe(result.day, 15);
}

// era only (no eraYear) must throw TypeError ("Insufficient fields")
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "japanese");
    shouldThrow(() => zdt.with({ era: "showa", month: 1, day: 5 }), TypeError);
}

// eraYear only (no era) must throw TypeError ("Insufficient fields")
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "japanese");
    shouldThrow(() => zdt.with({ eraYear: 50, month: 1, day: 5 }), TypeError);
}

// --- Gregory calendar era test ---
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "gregory");
    const result = zdt.with({ era: "ce", eraYear: 2024, month: 6, day: 1 });
    shouldBe(result.year, 2024);
    shouldBe(result.era, "ce");
    shouldBe(result.eraYear, 2024);
}

// era+eraYear+year that DON'T agree must throw RangeError ("Inconsistent year")
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "japanese");
    shouldThrow(() => zdt.with({ era: "showa", eraYear: 50, year: 2000, month: 1, day: 5 }), RangeError);
}

// year=0 (ISO year 0 = 1 BC) with conflicting era+eraYear must also throw RangeError.
// Requires optional<int32_t> year in calendarDateFromFields — a 0-sentinel would silently skip.
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "gregory");
    shouldThrow(() => zdt.with({ era: "ce", eraYear: 2024, year: 0, month: 1, day: 5 }), RangeError);
}

// year=0 consistent with bce eraYear=1 (BCE 1 = ISO 0) must NOT throw.
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC", "gregory");
    const result = zdt.with({ era: "bce", eraYear: 1, year: 0, month: 6, day: 15 });
    shouldBe(result.year, 0);
    shouldBe(result.era, "bce");
    shouldBe(result.eraYear, 1);
}

// --- iso8601 (no eras): with() still works correctly ---
{
    const zdt = new Temporal.ZonedDateTime(0n, "UTC");
    shouldBe(zdt.calendarId, "iso8601");
    const result = zdt.with({ year: 2024, month: 3, day: 15 });
    shouldBe(result.year, 2024);
    shouldBe(result.month, 3);
    shouldBe(result.day, 15);
}
