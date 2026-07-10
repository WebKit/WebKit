//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${String(expected)}, got ${String(actual)}`);
}

// Instant — cold via Temporal.Now.instant().
{
    const inst = Temporal.Now.instant();
    shouldBe(inst.constructor.name, "Instant", "instant().constructor.name");
    shouldBe(inst.constructor, Temporal.Instant, "instant().constructor === Temporal.Instant");
}

// PlainDate — cold via Temporal.Now.plainDateISO().
{
    const pd = Temporal.Now.plainDateISO();
    shouldBe(pd.constructor.name, "PlainDate", "plainDateISO().constructor.name");
    shouldBe(pd.constructor, Temporal.PlainDate, "plainDateISO().constructor === Temporal.PlainDate");
}

// PlainDateTime — cold via Temporal.Now.plainDateTimeISO().
{
    const pdt = Temporal.Now.plainDateTimeISO();
    shouldBe(pdt.constructor.name, "PlainDateTime", "plainDateTimeISO().constructor.name");
    shouldBe(pdt.constructor, Temporal.PlainDateTime, "plainDateTimeISO().constructor === Temporal.PlainDateTime");
}

// PlainTime — cold via Temporal.Now.plainTimeISO().
{
    const pt = Temporal.Now.plainTimeISO();
    shouldBe(pt.constructor.name, "PlainTime", "plainTimeISO().constructor.name");
    shouldBe(pt.constructor, Temporal.PlainTime, "plainTimeISO().constructor === Temporal.PlainTime");
}

// ZonedDateTime — cold via Temporal.Now.zonedDateTimeISO().
{
    const zdt = Temporal.Now.zonedDateTimeISO();
    shouldBe(zdt.constructor.name, "ZonedDateTime", "zonedDateTimeISO().constructor.name");
    shouldBe(zdt.constructor, Temporal.ZonedDateTime, "zonedDateTimeISO().constructor === Temporal.ZonedDateTime");
}

// PlainYearMonth — no Now factory, but reachable via PlainDate.toPlainYearMonth()
// without touching Temporal.PlainYearMonth.
{
    const pym = Temporal.Now.plainDateISO().toPlainYearMonth();
    shouldBe(pym.constructor.name, "PlainYearMonth", "toPlainYearMonth().constructor.name");
    shouldBe(pym.constructor, Temporal.PlainYearMonth, "toPlainYearMonth().constructor === Temporal.PlainYearMonth");
}

// PlainMonthDay — reachable via PlainDate.toPlainMonthDay().
{
    const pmd = Temporal.Now.plainDateISO().toPlainMonthDay();
    shouldBe(pmd.constructor.name, "PlainMonthDay", "toPlainMonthDay().constructor.name");
    shouldBe(pmd.constructor, Temporal.PlainMonthDay, "toPlainMonthDay().constructor === Temporal.PlainMonthDay");
}

// Duration — no cold path (no factory that skips Temporal.Duration), but still
// verify the prototype.constructor invariant.
{
    const dur = new Temporal.Duration(1);
    shouldBe(dur.constructor.name, "Duration", "Duration().constructor.name");
    shouldBe(dur.constructor, Temporal.Duration, "Duration().constructor === Temporal.Duration");
}
