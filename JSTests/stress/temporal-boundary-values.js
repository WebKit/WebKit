//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

function shouldThrow(fn, msg) {
    try { fn(); throw new Error(`${msg}: should have thrown`); }
    catch (e) { if (e.message.startsWith(msg)) throw e; }
}

{
    const maxNs = 8640000000000000000000n;
    const zdt = new Temporal.ZonedDateTime(maxNs, "UTC");
    shouldBe(zdt.epochNanoseconds, maxNs, "max epoch ns");
}
{
    const minNs = -8640000000000000000000n;
    const zdt = new Temporal.ZonedDateTime(minNs, "UTC");
    shouldBe(zdt.epochNanoseconds, minNs, "min epoch ns");
}
{
    shouldThrow(() => new Temporal.ZonedDateTime(8640000000000000000001n, "UTC"), "Beyond max epoch");
}
{
    shouldThrow(() => new Temporal.ZonedDateTime(-8640000000000000000001n, "UTC"), "Beyond min epoch");
}


{
    const pd = new Temporal.PlainDate(275760, 9, 13);
    shouldBe(pd.year, 275760, "max year");
}

{
    const pd = new Temporal.PlainDate(-271821, 4, 20);
    shouldBe(pd.year, -271821, "min year");
}

{
    const d = new Temporal.Duration(0, 0, 0, 0, 0, 0, 0, 0, 0, 1000000000);
    shouldBe(d.nanoseconds, 1000000000, "large nanoseconds");
    shouldBe(d.sign, 1, "positive sign");
}

{
    const d = new Temporal.Duration();
    shouldBe(d.blank, true, "zero duration is blank");
    shouldBe(d.sign, 0, "zero duration sign");
}

{
    const d = new Temporal.Duration(-1, -2, -3);
    shouldBe(d.sign, -1, "negative duration sign");
    shouldBe(d.blank, false, "negative duration not blank");
    const neg = d.negated();
    shouldBe(neg.years, 1, "negated years");
    shouldBe(neg.sign, 1, "negated sign");
}

{
    const d = new Temporal.Duration(-5, 0, 0, -3);
    const a = d.abs();
    shouldBe(a.years, 5, "abs years");
    shouldBe(a.days, 3, "abs days");
    shouldBe(a.sign, 1, "abs sign");
}

// --- Instant boundaries ---
{
    const maxNs = 8640000000000000000000n;
    const inst = Temporal.Instant.fromEpochNanoseconds(maxNs);
    shouldBe(inst.epochNanoseconds, maxNs, "instant max ns");
}

{
    shouldThrow(
        () => Temporal.Instant.fromEpochNanoseconds(8640000000000000000001n),
        "instant beyond max"
    );
}

{
    const t = new Temporal.PlainTime(23, 59, 59, 999, 999, 999);
    shouldBe(t.hour, 23, "max hour");
    shouldBe(t.nanosecond, 999, "max nanosecond");
}

{
    shouldThrow(
        () => new Temporal.PlainTime(24, 0, 0),
        "hour 24 invalid"
    );
}
