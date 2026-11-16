//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected "${expected}" but got "${actual}"`);
}
function shouldThrow(fn, errorType, msg) {
    let caught;
    try { fn(); } catch (e) { caught = e; }
    if (!caught) throw new Error(`${msg}: expected ${errorType.name}, no throw`);
    if (!(caught instanceof errorType))
        throw new Error(`${msg}: expected ${errorType.name} but got ${caught.constructor.name}`);
}

// ------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------
{
    // Spec: `new Temporal.PlainYearMonth(2024, 3)` → 2024-03.
    shouldBe(new Temporal.PlainYearMonth(2024, 3).toString(), "2024-03", "ctor 2-arg");
    shouldBe(new Temporal.PlainYearMonth(2024, 3, "iso8601").toString(), "2024-03", "ctor 3-arg iso");
    shouldBe(new Temporal.PlainYearMonth(2024, 3, "iso8601", 15).toString(), "2024-03", "ctor 4-arg ignores day in toString");

    // Missing year/month → undefined → ToIntegerWithTruncation throws RangeError.
    shouldThrow(() => new Temporal.PlainYearMonth(), RangeError, "ctor no args → RangeError");
    shouldThrow(() => new Temporal.PlainYearMonth(2024), RangeError, "ctor 1 arg → RangeError");

    // Non-finite throws RangeError.
    shouldThrow(() => new Temporal.PlainYearMonth(NaN, 3), RangeError, "ctor NaN year");
    shouldThrow(() => new Temporal.PlainYearMonth(Infinity, 3), RangeError, "ctor Infinity year");
    shouldThrow(() => new Temporal.PlainYearMonth(2024, NaN), RangeError, "ctor NaN month");
    shouldThrow(() => new Temporal.PlainYearMonth(2024, 13), RangeError, "ctor month 13");
    shouldThrow(() => new Temporal.PlainYearMonth(2024, 0), RangeError, "ctor month 0");

    // Calendar arg must be string or undefined.
    shouldThrow(() => new Temporal.PlainYearMonth(2024, 3, 42), TypeError, "ctor non-string calendar");
    shouldThrow(() => new Temporal.PlainYearMonth(2024, 3, "not-a-cal"), RangeError, "ctor bad calendar id");
}

// ------------------------------------------------------------------
// from()
// ------------------------------------------------------------------
{
    // String forms.
    shouldBe(Temporal.PlainYearMonth.from("2024-03").toString(), "2024-03", "from short form");
    shouldBe(Temporal.PlainYearMonth.from("2024-03-15").toString(), "2024-03", "from full-date form");

    // Object form.
    shouldBe(Temporal.PlainYearMonth.from({ year: 2024, month: 3 }).toString(), "2024-03", "from object year+month");
    shouldBe(Temporal.PlainYearMonth.from({ year: 2024, monthCode: "M03" }).toString(), "2024-03", "from object monthCode");

    // PYM-inherits (returns a new instance).
    const src = new Temporal.PlainYearMonth(2024, 3);
    const clone = Temporal.PlainYearMonth.from(src);
    shouldBe(clone.toString(), "2024-03", "from PYM clone");
    if (clone === src) throw new Error("from() must return a new instance, not the input");

    // Non-object non-string → TypeError.
    shouldThrow(() => Temporal.PlainYearMonth.from(42), TypeError, "from number");
    shouldThrow(() => Temporal.PlainYearMonth.from(true), TypeError, "from boolean");
    shouldThrow(() => Temporal.PlainYearMonth.from(null), TypeError, "from null");

    // Bad string.
    shouldThrow(() => Temporal.PlainYearMonth.from("not-a-date"), RangeError, "from garbage string");
}

// ------------------------------------------------------------------
// compare()
// ------------------------------------------------------------------
{
    const a = Temporal.PlainYearMonth.from("2024-03");
    const b = Temporal.PlainYearMonth.from("2024-06");
    shouldBe(Temporal.PlainYearMonth.compare(a, b), -1, "compare a<b");
    shouldBe(Temporal.PlainYearMonth.compare(b, a), 1, "compare b>a");
    shouldBe(Temporal.PlainYearMonth.compare(a, a), 0, "compare a==a");
}

// ------------------------------------------------------------------
// equals()
// ------------------------------------------------------------------
{
    const a = Temporal.PlainYearMonth.from("2024-03");
    shouldBe(a.equals(Temporal.PlainYearMonth.from("2024-03")), true, "equals same YM");
    shouldBe(a.equals(Temporal.PlainYearMonth.from("2024-04")), false, "equals different month");
    shouldBe(a.equals(Temporal.PlainYearMonth.from("2023-03")), false, "equals different year");

    // Different calendars → equals returns false (not throw).
    const iso = Temporal.PlainYearMonth.from({ year: 2024, month: 3 });
    const heb = Temporal.PlainYearMonth.from({ year: 5784, month: 6, calendar: "hebrew" });
    shouldBe(iso.equals(heb), false, "equals different calendars");
}

// ------------------------------------------------------------------
// add() / subtract()  — ISO
// ------------------------------------------------------------------
{
    const a = Temporal.PlainYearMonth.from("2024-03");
    shouldBe(a.add({ months: 5 }).toString(), "2024-08", "add 5 months");
    shouldBe(a.add({ months: 12 }).toString(), "2025-03", "add 12 months");
    shouldBe(a.add({ years: 1 }).toString(), "2025-03", "add 1 year");
    shouldBe(a.add({ years: 1, months: 3 }).toString(), "2025-06", "add 1y+3m");

    // Year-boundary crossing
    shouldBe(a.add({ months: 10 }).toString(), "2025-01", "add 10 months (year boundary)");
    shouldBe(a.add({ months: 22 }).toString(), "2026-01", "add 22 months (2 year boundaries)");

    // subtract
    shouldBe(a.subtract({ months: 2 }).toString(), "2024-01", "subtract 2 months");
    shouldBe(a.subtract({ months: 3 }).toString(), "2023-12", "subtract 3 months (year boundary)");
    shouldBe(a.subtract({ years: 1 }).toString(), "2023-03", "subtract 1 year");

    // Sub-month units must be zero (weeks/days/hours/…/nanoseconds).
    shouldThrow(() => a.add({ days: 1 }), RangeError, "add with days → RangeError");
    shouldThrow(() => a.add({ weeks: 1 }), RangeError, "add with weeks → RangeError");
    shouldThrow(() => a.add({ hours: 1 }), RangeError, "add with hours → RangeError");
    shouldThrow(() => a.add({ nanoseconds: 1 }), RangeError, "add with nanoseconds → RangeError");
}

// ------------------------------------------------------------------
// until() / since()
// ------------------------------------------------------------------
{
    const jan = Temporal.PlainYearMonth.from("2024-01");
    const jul = Temporal.PlainYearMonth.from("2024-07");
    const nextYear = Temporal.PlainYearMonth.from("2026-03");

    // Default (largestUnit = auto → year).
    shouldBe(jan.until(jul).toString(), "P6M", "until default (6 months)");
    shouldBe(jan.until(nextYear).toString(), "P2Y2M", "until 2y2m");

    // Explicit largestUnit
    shouldBe(jan.until(nextYear, { largestUnit: "month" }).toString(), "P26M", "until largestUnit=month");
    shouldBe(jan.until(nextYear, { largestUnit: "year" }).toString(), "P2Y2M", "until largestUnit=year");

    // since = -until
    shouldBe(jul.since(jan).toString(), "P6M", "since 6 months forward");
    shouldBe(jan.since(jul).toString(), "-P6M", "since 6 months backward");

    // Equal → zero-duration
    shouldBe(jan.until(jan).toString(), "PT0S", "until self = zero");
    shouldBe(jan.since(jan).toString(), "PT0S", "since self = zero");

    // Cross-calendar → RangeError
    const iso = Temporal.PlainYearMonth.from({ year: 2024, month: 3 });
    const heb = Temporal.PlainYearMonth.from({ year: 5784, month: 6, calendar: "hebrew" });
    shouldThrow(() => iso.until(heb), RangeError, "until across calendars");
    shouldThrow(() => iso.since(heb), RangeError, "since across calendars");

    // Time-category largestUnit not allowed
    shouldThrow(() => jan.until(jul, { largestUnit: "day" }), RangeError, "until largestUnit=day");
    shouldThrow(() => jan.until(jul, { largestUnit: "hour" }), RangeError, "until largestUnit=hour");
    shouldThrow(() => jan.until(jul, { smallestUnit: "week" }), RangeError, "until smallestUnit=week");
}

// ------------------------------------------------------------------
// with()
// ------------------------------------------------------------------
{
    const a = Temporal.PlainYearMonth.from("2024-03");
    shouldBe(a.with({ month: 8 }).toString(), "2024-08", "with month");
    shouldBe(a.with({ year: 2030 }).toString(), "2030-03", "with year");
    shouldBe(a.with({ year: 2030, month: 12 }).toString(), "2030-12", "with year+month");
    shouldBe(a.with({ monthCode: "M06" }).toString(), "2024-06", "with monthCode");

    // First arg must be an object with at least one field.
    shouldThrow(() => a.with(42), TypeError, "with non-object");
    shouldThrow(() => a.with({}), TypeError, "with empty object");
    // calendar / timeZone props rejected (spec Step 3: IsPartialTemporalObject).
    shouldThrow(() => a.with({ calendar: "iso8601", month: 5 }), TypeError, "with calendar prop");
    shouldThrow(() => a.with({ timeZone: "UTC", month: 5 }), TypeError, "with timeZone prop");

    // Overflow option.
    shouldBe(a.with({ month: 13 }, { overflow: "constrain" }).toString(), "2024-12", "with constrain");
    shouldThrow(() => a.with({ month: 13 }, { overflow: "reject" }), RangeError, "with reject bad month");
}

// ------------------------------------------------------------------
// toPlainDate()
// ------------------------------------------------------------------
{
    const a = Temporal.PlainYearMonth.from("2024-03");
    shouldBe(a.toPlainDate({ day: 15 }).toString(), "2024-03-15", "toPlainDate day=15");
    shouldBe(a.toPlainDate({ day: 1 }).toString(), "2024-03-01", "toPlainDate day=1");

    // Day must be present.
    shouldThrow(() => a.toPlainDate({}), TypeError, "toPlainDate no day");
    shouldThrow(() => a.toPlainDate(42), TypeError, "toPlainDate non-object");

    // Day must be a POSITIVE integer per spec (PrepareCalendarFields with day in required-positive list).
    // Regression test for a JSC bug where day ≤ 0 silently constrained to day=1.
    shouldThrow(() => a.toPlainDate({ day: 0 }), RangeError, "toPlainDate day=0 → RangeError");
    shouldThrow(() => a.toPlainDate({ day: -5 }), RangeError, "toPlainDate day=-5 → RangeError");
    shouldThrow(() => a.toPlainDate({ day: -0.5 }), RangeError, "toPlainDate day=-0.5 → RangeError (truncates to 0)");
    shouldThrow(() => a.toPlainDate({ day: NaN }), RangeError, "toPlainDate day=NaN → RangeError");
    shouldThrow(() => a.toPlainDate({ day: Infinity }), RangeError, "toPlainDate day=Infinity → RangeError");

    // Overflow behavior: day exceeding month length constrains to last valid day.
    const feb = Temporal.PlainYearMonth.from("2024-02");
    shouldBe(feb.toPlainDate({ day: 31 }).toString(), "2024-02-29", "toPlainDate day=31 in leap Feb");
    shouldBe(feb.toPlainDate({ day: 30 }).toString(), "2024-02-29", "toPlainDate day=30 in leap Feb");
    const feb2023 = Temporal.PlainYearMonth.from("2023-02");
    shouldBe(feb2023.toPlainDate({ day: 29 }).toString(), "2023-02-28", "toPlainDate day=29 in non-leap Feb");
}

// ------------------------------------------------------------------
// Non-ISO calendar (Hebrew) — spot check for calendar dispatch
// ------------------------------------------------------------------
{
    const heb = Temporal.PlainYearMonth.from({ year: 5784, monthCode: "M06", calendar: "hebrew" });
    shouldBe(heb.calendarId, "hebrew", "hebrew calendarId");
    shouldBe(heb.monthCode, "M06", "hebrew monthCode");
    // year/month reflect calendar-coordinate values, not ISO ones.
    shouldBe(typeof heb.year, "number", "hebrew year is number");
    shouldBe(typeof heb.month, "number", "hebrew month is number");
    shouldBe(heb.monthsInYear >= 12, true, "hebrew monthsInYear ≥ 12");

    // Hebrew leap year has 13 months; non-leap has 12.
    const monthsInYear = heb.monthsInYear;
    if (monthsInYear !== 12 && monthsInYear !== 13)
        throw new Error(`Hebrew monthsInYear must be 12 or 13, got ${monthsInYear}`);
}

// ------------------------------------------------------------------
// Lunisolar year-add constrain — regression test for a JSC bug
// where Chinese calendar M{n}L + 1Y into a non-leap year constrained
// to M12 (last month) instead of M{n} (base month, matching V8/icu4x).
// ------------------------------------------------------------------
{
    // Chinese 2023 has leap M02L; 2024 has none.
    const m02L = Temporal.PlainYearMonth.from({ year: 2023, monthCode: "M02L", calendar: "chinese" });
    shouldBe(m02L.add({ years: 1 }).monthCode, "M02", "Chi 2023 M02L +1Y constrains to M02 (was: M12)");

    // Chinese 2020 M04L → 2021 no leap → M04.
    const m04L = Temporal.PlainYearMonth.from({ year: 2020, monthCode: "M04L", calendar: "chinese" });
    shouldBe(m04L.add({ years: 1 }).monthCode, "M04", "Chi 2020 M04L +1Y constrains to M04");

    // Chinese 2014 M09L → 2015 no leap → M09.
    const m09L = Temporal.PlainYearMonth.from({ year: 2014, monthCode: "M09L", calendar: "chinese" });
    shouldBe(m09L.add({ years: 1 }).monthCode, "M09", "Chi 2014 M09L +1Y constrains to M09");
}

// ------------------------------------------------------------------
// Non-ISO PYM.from requires month/monthCode — regression test for a
// JSC bug where non-ISO calendars silently defaulted month to 1
// when only era+eraYear were provided.
// ------------------------------------------------------------------
{
    // Non-ISO: era+eraYear without month/monthCode → TypeError.
    shouldThrow(() => Temporal.PlainYearMonth.from({ era: "ce", eraYear: 2024, calendar: "gregory" }),
        TypeError, "gregory era+eraYear without month → TypeError");
    shouldThrow(() => Temporal.PlainYearMonth.from({ era: "reiwa", eraYear: 5, calendar: "japanese" }),
        TypeError, "japanese era+eraYear without month → TypeError");
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: 5784, calendar: "hebrew" }),
        TypeError, "hebrew year without month → TypeError");
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: 2023, calendar: "chinese" }),
        TypeError, "chinese year without month → TypeError");

    // Sanity: with month still works.
    shouldBe(Temporal.PlainYearMonth.from({ era: "ce", eraYear: 2024, month: 3, calendar: "gregory" }).toString(),
        "2024-03-01[u-ca=gregory]", "gregory era+eraYear+month OK");
}

// ------------------------------------------------------------------
// toString options — calendar annotation
// ------------------------------------------------------------------
{
    const iso = Temporal.PlainYearMonth.from("2024-03");
    shouldBe(iso.toString(), "2024-03", "ISO toString default");
    shouldBe(iso.toString({ calendarName: "always" }), "2024-03-01[u-ca=iso8601]", "ISO calendarName=always");
    shouldBe(iso.toString({ calendarName: "never" }), "2024-03", "ISO calendarName=never");

    const heb = Temporal.PlainYearMonth.from({ year: 5784, month: 6, calendar: "hebrew" });
    // Non-ISO calendars always print calendar annotation by default (auto).
    if (!heb.toString().includes("[u-ca=hebrew]"))
        throw new Error(`Hebrew default toString must include [u-ca=hebrew], got: ${heb.toString()}`);
}

// ------------------------------------------------------------------
// valueOf → TypeError
// ------------------------------------------------------------------
{
    const a = Temporal.PlainYearMonth.from("2024-03");
    shouldThrow(() => a.valueOf(), TypeError, "valueOf throws");
    // Any arithmetic coercion triggers valueOf.
    shouldThrow(() => a > a, TypeError, "> coercion throws via valueOf");
}

// ------------------------------------------------------------------
// Year-limit boundaries — regression test for a JSC debug-assertion
// crash where `from({year: 275761, ...})` reached ISO8601::PlainDate
// ctor with year > maxYear before the limits check.
// ------------------------------------------------------------------
{
    // Spec (ISOYearMonthWithinLimits): year in [-271821, 275760] with
    // month>=4 at min year, month<=9 at max year (matches V8).
    // Within limits — must construct OK.
    shouldBe(Temporal.PlainYearMonth.from({ year: 275760, month: 9 }).toString(),
        "+275760-09", "year=maxYear (275760) month=9 OK");
    shouldBe(Temporal.PlainYearMonth.from({ year: -271821, month: 4 }).toString(),
        "-271821-04", "year=minYear (-271821) month=4 OK");

    // Just outside boundaries — must throw RangeError, not crash.
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: 275760, month: 10 }),
        RangeError, "year=275760 month=10 exceeds YearMonth range");
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: 275761, month: 1 }),
        RangeError, "year=275761 exceeds YearMonth range (was: debug assert crash)");
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: -271821, month: 3 }),
        RangeError, "year=-271821 month=3 exceeds YearMonth range");
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: -271822, month: 6 }),
        RangeError, "year=-271822 exceeds YearMonth range");

    // Constructor path — same story: `new PlainYearMonth(275761, 1)`
    // must throw RangeError, not assert.
    shouldThrow(() => new Temporal.PlainYearMonth(275761, 1),
        RangeError, "ctor year=275761 → RangeError");
    shouldThrow(() => new Temporal.PlainYearMonth(-271822, 6),
        RangeError, "ctor year=-271822 → RangeError");

    // Sibling bug: PlainDate.from ISO path had the same ordering issue
    // in dateFromFields — PlainDate ctor asserted before isDateTimeWithinLimits.
    shouldThrow(() => Temporal.PlainDate.from({ year: 275761, month: 1, day: 1 }),
        RangeError, "PD year=275761 → RangeError (was: debug assert crash)");
    shouldThrow(() => Temporal.PlainDate.from({ year: -271822, month: 6, day: 1 }),
        RangeError, "PD year=-271822 → RangeError");
    shouldThrow(() => new Temporal.PlainDate(275761, 1, 1),
        RangeError, "PD ctor year=275761 → RangeError");
}
