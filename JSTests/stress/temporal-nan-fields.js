//@ requireOptions("--useTemporal=1")

function shouldThrow(fn, type) {
    let err;
    try { fn(); } catch (e) { err = e; }
    if (!(err instanceof type))
        throw new Error(`Expected ${type.name} but got ${err}`);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Spec uses ToIntegerWithTruncation (throws on NaN/+/-Infinity) for year, eraYear, and time
// fields; ToPositiveIntegerWithTruncation (also rejects ≤0) for month and day. Previously
// JSC used toIntegerOrInfinity which silently maps NaN -> 0, letting NaN inputs slip through
// across PlainDate/PlainTime/PlainMonthDay/PlainYearMonth/PlainDateTime/ZonedDateTime/Duration.

// ---- PlainDate.from ----
shouldThrow(() => Temporal.PlainDate.from({ year: NaN, month: 1, day: 1 }), RangeError);
shouldThrow(() => Temporal.PlainDate.from({ year: 2024, month: NaN, day: 1 }), RangeError);
shouldThrow(() => Temporal.PlainDate.from({ year: 2024, month: 1, day: NaN }), RangeError);
shouldThrow(() => Temporal.PlainDate.from({ era: "ce", eraYear: NaN, month: 1, day: 1, calendar: "gregory" }), RangeError);

// ---- PlainDate.prototype.with ----
{
    const pd = Temporal.PlainDate.from("2024-01-15");
    shouldThrow(() => pd.with({ year: NaN }), RangeError);
    shouldThrow(() => pd.with({ month: NaN }), RangeError);
    shouldThrow(() => pd.with({ day: NaN }), RangeError);
}

// ---- PlainDateTime.prototype.with ----
{
    const pdt = Temporal.PlainDateTime.from("2024-01-15T12:30:45");
    shouldThrow(() => pdt.with({ year: NaN }), RangeError);
    shouldThrow(() => pdt.with({ month: NaN }), RangeError);
    shouldThrow(() => pdt.with({ day: NaN }), RangeError);
    shouldThrow(() => pdt.with({ hour: NaN }), RangeError);
    shouldThrow(() => pdt.with({ minute: NaN }), RangeError);
    shouldThrow(() => pdt.with({ second: NaN }), RangeError);
    shouldThrow(() => pdt.with({ millisecond: NaN }), RangeError);
    shouldThrow(() => pdt.with({ microsecond: NaN }), RangeError);
    shouldThrow(() => pdt.with({ nanosecond: NaN }), RangeError);
    const jpdt = pdt.withCalendar("japanese");
    shouldThrow(() => jpdt.with({ eraYear: NaN }), RangeError);
}

// ---- PlainTime.from / .with (all six time fields) ----
shouldThrow(() => Temporal.PlainTime.from({ hour: NaN }), RangeError);
shouldThrow(() => Temporal.PlainTime.from({ minute: NaN }), RangeError);
shouldThrow(() => Temporal.PlainTime.from({ second: NaN }), RangeError);
shouldThrow(() => Temporal.PlainTime.from({ millisecond: NaN }), RangeError);
shouldThrow(() => Temporal.PlainTime.from({ microsecond: NaN }), RangeError);
shouldThrow(() => Temporal.PlainTime.from({ nanosecond: NaN }), RangeError);
{
    const pt = Temporal.PlainTime.from({ hour: 12 });
    shouldThrow(() => pt.with({ hour: NaN }), RangeError);
    shouldThrow(() => pt.with({ minute: NaN }), RangeError);
    shouldThrow(() => pt.with({ second: NaN }), RangeError);
    shouldThrow(() => pt.with({ millisecond: NaN }), RangeError);
    shouldThrow(() => pt.with({ microsecond: NaN }), RangeError);
    shouldThrow(() => pt.with({ nanosecond: NaN }), RangeError);
}

// ---- PlainMonthDay.from / .with ----
shouldThrow(() => Temporal.PlainMonthDay.from({ month: NaN, day: 1 }), RangeError);
shouldThrow(() => Temporal.PlainMonthDay.from({ month: 1, day: NaN }), RangeError);
shouldThrow(() => Temporal.PlainMonthDay.from({ era: "ce", eraYear: NaN, month: 1, day: 1, calendar: "gregory" }), RangeError);
{
    const pmd = Temporal.PlainMonthDay.from({ month: 5, day: 15 });
    shouldThrow(() => pmd.with({ day: NaN }), RangeError);
    shouldThrow(() => pmd.with({ month: NaN, day: 15 }), RangeError);
    const jpmd = Temporal.PlainMonthDay.from({ monthCode: "M05", day: 15, calendar: "japanese" });
    shouldThrow(() => jpmd.with({ eraYear: NaN }), RangeError);
}

// ---- PlainYearMonth.from / .with ----
shouldThrow(() => Temporal.PlainYearMonth.from({ year: NaN, month: 1 }), RangeError);
shouldThrow(() => Temporal.PlainYearMonth.from({ year: 2024, month: NaN }), RangeError);
shouldThrow(() => Temporal.PlainYearMonth.from({ era: "ce", eraYear: NaN, month: 1, calendar: "gregory" }), RangeError);
{
    const pym = Temporal.PlainYearMonth.from("2024-01");
    shouldThrow(() => pym.with({ year: NaN }), RangeError);
    shouldThrow(() => pym.with({ month: NaN }), RangeError);
}

// ---- ZonedDateTime.from / .with (calendar fields, time fields, era fields) ----
shouldThrow(() => Temporal.ZonedDateTime.from({ year: NaN, month: 1, day: 1, timeZone: "UTC" }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ year: 2024, month: NaN, day: 1, timeZone: "UTC" }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: NaN, timeZone: "UTC" }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: 1, hour: NaN, timeZone: "UTC" }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: 1, minute: NaN, timeZone: "UTC" }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: 1, second: NaN, timeZone: "UTC" }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: 1, millisecond: NaN, timeZone: "UTC" }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: 1, microsecond: NaN, timeZone: "UTC" }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: 1, nanosecond: NaN, timeZone: "UTC" }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ era: "ce", eraYear: NaN, month: 1, day: 1, calendar: "gregory", timeZone: "UTC" }), RangeError);
{
    const zdt = Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: 1, timeZone: "UTC" });
    shouldThrow(() => zdt.with({ year: NaN }), RangeError);
    shouldThrow(() => zdt.with({ hour: NaN }), RangeError);
    shouldThrow(() => zdt.with({ nanosecond: NaN }), RangeError);
}

// ---- Duration relativeTo: nested date object property reads via TemporalDuration.cpp ----
{
    const dur = Temporal.Duration.from({ days: 5 });
    shouldThrow(() => dur.round({ largestUnit: "weeks", relativeTo: { year: NaN, month: 1, day: 1 } }), RangeError);
    shouldThrow(() => dur.round({ largestUnit: "weeks", relativeTo: { year: 2024, month: NaN, day: 1 } }), RangeError);
    shouldThrow(() => dur.round({ largestUnit: "weeks", relativeTo: { year: 2024, month: 1, day: NaN } }), RangeError);
    shouldThrow(() => dur.round({ largestUnit: "weeks", relativeTo: { year: 2024, month: 1, day: 1, hour: NaN, timeZone: "UTC" } }), RangeError);
    shouldThrow(() => dur.round({ largestUnit: "weeks", relativeTo: { year: 2024, month: 1, day: 1, nanosecond: NaN, timeZone: "UTC" } }), RangeError);
    shouldThrow(() => dur.round({ largestUnit: "weeks", relativeTo: { era: "ce", eraYear: NaN, month: 1, day: 1, calendar: "gregory" } }), RangeError);
}

// ---- +/-Infinity continues to throw at the same sites ----
shouldThrow(() => Temporal.PlainDate.from({ year: Infinity, month: 1, day: 1 }), RangeError);
shouldThrow(() => Temporal.PlainTime.from({ hour: -Infinity }), RangeError);
shouldThrow(() => Temporal.PlainYearMonth.from({ year: Infinity, month: 1 }), RangeError);
shouldThrow(() => Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: 1, nanosecond: Infinity, timeZone: "UTC" }), RangeError);

// ---- Sanity: valid integer-like values still succeed (regression guard) ----
shouldBe(Temporal.PlainDate.from({ year: 2024, month: 1, day: 1 }).toString(), "2024-01-01");
shouldBe(Temporal.PlainTime.from({ hour: 1.7, minute: 2.9 }).toString(), "01:02:00");
shouldBe(Temporal.PlainYearMonth.from({ year: 2024, month: 5 }).toString(), "2024-05");
shouldBe(Temporal.PlainMonthDay.from({ month: 5, day: 15 }).toString(), "05-15");
shouldBe(Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1 }).toString(), "2024-01-01T00:00:00");
shouldBe(Temporal.ZonedDateTime.from({ year: 2024, month: 1, day: 1, timeZone: "UTC" }).toString(), "2024-01-01T00:00:00+00:00[UTC]");
// Year 0 and negative year are valid (ToIntegerWithTruncation, not ToPositive).
shouldBe(Temporal.PlainDate.from({ year: 0, month: 1, day: 1 }).year, 0);
shouldBe(Temporal.PlainDate.from({ year: -1, month: 1, day: 1 }).year, -1);
