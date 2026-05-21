//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

function shouldThrow(fn, msg) {
    try { fn(); throw new Error(`NOTHROW: ${msg}`); }
    catch (e) { if (e.message.startsWith("NOTHROW:")) throw e; }
}

// --- Offset string formats ---
{
    const inst = Temporal.Instant.from("2024-01-01T00:00:00+05:30");
    shouldBe(inst.epochNanoseconds, 1704047400000000000n, "+05:30 offset");

    const inst2 = Temporal.Instant.from("2024-01-01T00:00:00-12:00");
    const inst2Ns = inst2.epochNanoseconds;
    shouldBe(inst2Ns > 1704067200000000000n, true, "-12:00 is after UTC midnight");

    const inst3 = Temporal.Instant.from("2024-01-01T00:00:00Z");
    shouldBe(inst3.epochNanoseconds, 1704067200000000000n, "Z offset");
}

// --- Sub-second nanosecond precision in strings ---
{
    const inst = Temporal.Instant.from("2024-01-01T00:00:00.123456789Z");
    shouldBe(inst.epochNanoseconds, 1704067200123456789n, "9-digit nanosecond precision");

    const zdt = Temporal.ZonedDateTime.from("2024-01-01T00:00:00.000000001[UTC]");
    shouldBe(zdt.nanosecond, 1, "1 nanosecond precision");

    const zdt2 = Temporal.ZonedDateTime.from("2024-01-01T00:00:00.999999999[UTC]");
    shouldBe(zdt2.millisecond, 999, "999ms");
    shouldBe(zdt2.microsecond, 999, "999µs");
    shouldBe(zdt2.nanosecond, 999, "999ns");
}

// --- Calendar annotation in bracket ---
{
    const pd = Temporal.PlainDate.from("2024-03-15[u-ca=iso8601]");
    shouldBe(pd.calendarId, "iso8601", "calendar annotation iso8601");
    shouldBe(pd.year, 2024, "year with calendar annotation");
}

// --- Duration string parsing ---
{
    shouldBe(Temporal.Duration.from("P1Y").years, 1, "P1Y");
    shouldBe(Temporal.Duration.from("P1M").months, 1, "P1M");
    shouldBe(Temporal.Duration.from("P1W").weeks, 1, "P1W");
    shouldBe(Temporal.Duration.from("P1D").days, 1, "P1D");
    shouldBe(Temporal.Duration.from("PT1H").hours, 1, "PT1H");
    shouldBe(Temporal.Duration.from("PT1M").minutes, 1, "PT1M");
    shouldBe(Temporal.Duration.from("PT1S").seconds, 1, "PT1S");
    shouldBe(Temporal.Duration.from("PT0.5S").milliseconds, 500, "PT0.5S");
    shouldBe(Temporal.Duration.from("PT0.000000001S").nanoseconds, 1, "PT0.000000001S");
    shouldBe(Temporal.Duration.from("-P1D").days, -1, "negative duration");
    shouldBe(Temporal.Duration.from("P1Y2M3W4DT5H6M7.89S").years, 1, "full duration");
}

// --- Invalid strings should throw ---
{
    shouldThrow(() => Temporal.PlainDate.from(""), "empty string");
    shouldThrow(() => Temporal.PlainDate.from("not-a-date"), "invalid date");
    shouldThrow(() => Temporal.PlainDate.from("2024-13-01"), "month 13");
    shouldThrow(() => Temporal.PlainDate.from("2024-02-30"), "Feb 30");
    shouldThrow(() => Temporal.Instant.from("2024-01-01"), "Instant needs offset");
    shouldThrow(() => Temporal.Duration.from("P"), "empty duration");
    shouldThrow(() => Temporal.Duration.from("1Y"), "missing P prefix");
}

// --- Extended year format ---
{
    const pd = new Temporal.PlainDate(275760, 9, 13);
    shouldBe(pd.year, 275760, "max year 275760");

    const pd2 = new Temporal.PlainDate(-271821, 4, 20);
    shouldBe(pd2.year, -271821, "min year -271821");
}

// --- PlainTime string formats ---
{
    const t1 = Temporal.PlainTime.from("12:34:56.789");
    shouldBe(t1.hour, 12, "time hour");
    shouldBe(t1.millisecond, 789, "time ms");

    const t2 = Temporal.PlainTime.from("00:00");
    shouldBe(t2.hour, 0, "midnight");
    shouldBe(t2.minute, 0, "midnight min");

    const t3 = Temporal.PlainTime.from("23:59:59.999999999");
    shouldBe(t3.hour, 23, "max time hour");
    shouldBe(t3.nanosecond, 999, "max time ns");
}
