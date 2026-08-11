//@ requireOptions("--useTemporal=1")

// Temporal.PlainMonthDay — spec conformance stress test.
// Covers: from + equals + with + toPlainDate (ISO and non-ISO with era resolution) +
// toString options + valueOf trap. V8-cross-verified.

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
// Accept either RangeError or TypeError — both are spec-conformant for "invalid input".
function shouldThrowAny(fn, msg) {
    let caught;
    try { fn(); } catch (e) { caught = e; }
    if (!caught) throw new Error(`${msg}: expected throw, no throw`);
}

// ------------------------------------------------------------------
// from() — strings
// ------------------------------------------------------------------
{
    shouldBe(Temporal.PlainMonthDay.from("--06-15").toString(), "06-15", "from --MM-DD");
    shouldBe(Temporal.PlainMonthDay.from("02-29").toString(), "02-29", "from MM-DD (2-arg)");
    shouldBe(Temporal.PlainMonthDay.from("2024-06-15").toString(), "06-15", "from full-date form");
    shouldThrow(() => Temporal.PlainMonthDay.from("--02-30"), RangeError, "from invalid day");
    shouldThrow(() => Temporal.PlainMonthDay.from("--00-15"), RangeError, "from invalid month");
    shouldThrow(() => Temporal.PlainMonthDay.from(""), RangeError, "from empty");
    shouldThrow(() => Temporal.PlainMonthDay.from("garbage"), RangeError, "from garbage");
    shouldThrow(() => Temporal.PlainMonthDay.from(42), TypeError, "from number");
    shouldThrow(() => Temporal.PlainMonthDay.from(null), TypeError, "from null");
}

// ------------------------------------------------------------------
// from() — objects
// ------------------------------------------------------------------
{
    shouldBe(Temporal.PlainMonthDay.from({ month: 6, day: 15 }).toString(), "06-15", "from month+day");
    shouldBe(Temporal.PlainMonthDay.from({ monthCode: "M06", day: 15 }).toString(), "06-15", "from monthCode+day");
    // month and monthCode both present + agree
    shouldBe(Temporal.PlainMonthDay.from({ month: 6, monthCode: "M06", day: 15 }).toString(), "06-15", "from month+monthCode agreeing");
    // month and monthCode disagree → RangeError
    shouldThrow(() => Temporal.PlainMonthDay.from({ month: 6, monthCode: "M07", day: 15 }), RangeError, "from month/monthCode disagreement");
    // required fields absent
    shouldThrow(() => Temporal.PlainMonthDay.from({ day: 15 }), TypeError, "from without month/monthCode");
    shouldThrow(() => Temporal.PlainMonthDay.from({ monthCode: "M06" }), TypeError, "from without day");
    // PMD-instance clone
    const src = Temporal.PlainMonthDay.from("--06-15");
    const clone = Temporal.PlainMonthDay.from(src);
    shouldBe(clone.toString(), "06-15", "from PMD clone");
    if (clone === src) throw new Error("PMD.from must return a new instance");

    const hebSrc = Temporal.PlainMonthDay.from({ monthCode: "M03", day: 15, calendar: "hebrew" });
    const hebClone = Temporal.PlainMonthDay.from(hebSrc);
    shouldBe(hebClone.calendarId, "hebrew", "clone preserves hebrew calendar (Bug #11)");
    shouldBe(hebClone.toString(), hebSrc.toString(), "clone toString matches source (Bug #11)");
}

// ------------------------------------------------------------------
// from() — overflow
// ------------------------------------------------------------------
{
    // default constrain
    shouldBe(Temporal.PlainMonthDay.from({ month: 13, day: 15 }).toString(), "12-15", "from month=13 constrains to 12");
    shouldBe(Temporal.PlainMonthDay.from({ month: 2, day: 31 }).toString(), "02-29", "from Feb 31 constrains to Feb 29");
    // reject
    shouldThrow(() => Temporal.PlainMonthDay.from({ month: 13, day: 15 }, { overflow: "reject" }), RangeError, "from month=13 reject");
    shouldThrow(() => Temporal.PlainMonthDay.from({ month: 2, day: 31 }, { overflow: "reject" }), RangeError, "from Feb 31 reject");
}

// ------------------------------------------------------------------
// equals()
// ------------------------------------------------------------------
{
    const iso = Temporal.PlainMonthDay.from("--06-15");
    shouldBe(iso.equals(Temporal.PlainMonthDay.from("--06-15")), true, "equals same");
    shouldBe(iso.equals(Temporal.PlainMonthDay.from("--06-16")), false, "equals different day");
    shouldBe(iso.equals(Temporal.PlainMonthDay.from("--07-15")), false, "equals different month");
    shouldBe(iso.equals({ monthCode: "M06", day: 15 }), true, "equals object literal");
    // Cross-calendar
    const heb = Temporal.PlainMonthDay.from({ monthCode: "M06", day: 15, calendar: "hebrew" });
    shouldBe(iso.equals(heb), false, "equals across calendars");
}

// ------------------------------------------------------------------
// Non-ISO month/monthCode conflict — regression test for a JSC bug
// where non-ISO .with()/.from() on non-lunisolar calendars silently
// accepted disagreeing month + monthCode fields.
// ISO already rejected via resolveISOFields; the check was missing from
// the non-ISO branches of dateFromFields/yearMonthFromFields/monthDayFromFields.
// ------------------------------------------------------------------
{
    // Japanese calendar (Gregorian-based) — month ordinal == monthCode number.
    const jpmd = Temporal.PlainMonthDay.from({ monthCode: "M03", day: 15, calendar: "japanese" });
    const jpym = Temporal.PlainYearMonth.from({ year: 2023, monthCode: "M03", calendar: "japanese" });
    const jpd = Temporal.PlainDate.from({ year: 2023, monthCode: "M03", day: 15, calendar: "japanese" });
    shouldThrowAny(() => jpmd.with({ month: 3, monthCode: "M04" }),
        "PMD japanese .with month/monthCode conflict → throws");
    shouldThrowAny(() => jpym.with({ month: 3, monthCode: "M04" }),
        "PYM japanese .with month/monthCode conflict → throws");
    shouldThrowAny(() => jpd.with({ month: 3, monthCode: "M04" }),
        "PD japanese .with month/monthCode conflict → throws");
    shouldThrowAny(() => Temporal.PlainMonthDay.from({ month: 3, monthCode: "M04", day: 15, calendar: "japanese" }),
        "PMD japanese .from month/monthCode conflict → throws");

    // Lunisolar (chinese/dangi/hebrew): the direct month-vs-monthCode-number check is skipped
    // (in leap years the leap slot pushes ordinals up). Whether the merge subsequently accepts
    // both fields is engine-defined — JSC accepts, V8 throws "Insufficient fields". Both are
    // spec-conformant since spec's CalendarResolveFields for non-ISO is implementation-defined.
}

// ------------------------------------------------------------------
// with()
// ------------------------------------------------------------------
{
    const pmd = Temporal.PlainMonthDay.from("--06-15");
    shouldBe(pmd.with({ month: 8 }).toString(), "08-15", "with month");
    shouldBe(pmd.with({ monthCode: "M08" }).toString(), "08-15", "with monthCode");
    shouldBe(pmd.with({ day: 20 }).toString(), "06-20", "with day");
    // month/monthCode disagreement
    shouldThrow(() => pmd.with({ month: 8, monthCode: "M09" }), RangeError, "with month/monthCode disagree");
    // overflow
    shouldBe(pmd.with({ day: 31 }).toString(), "06-30", "with day=31 constrains to 30 (Jun)");
    shouldThrow(() => pmd.with({ day: 31 }, { overflow: "reject" }), RangeError, "with day=31 reject");

    // IsPartialTemporalObject rejections.
    shouldThrow(() => pmd.with(Temporal.PlainDate.from("2024-06-15")), TypeError, "with PlainDate");
    shouldThrow(() => pmd.with(Temporal.PlainMonthDay.from("--06-15")), TypeError, "with PlainMonthDay");
    shouldThrow(() => pmd.with({ calendar: "iso8601", day: 5 }), TypeError, "with calendar prop");
    shouldThrow(() => pmd.with({ timeZone: "UTC", day: 5 }), TypeError, "with timeZone prop");
    shouldThrow(() => pmd.with(42), TypeError, "with non-object");
}

// ------------------------------------------------------------------
// toPlainDate() — ISO
// ------------------------------------------------------------------
{
    const pmd = Temporal.PlainMonthDay.from("--06-15");
    shouldBe(pmd.toPlainDate({ year: 2024 }).toString(), "2024-06-15", "toPlainDate ISO year");
    // Empty object → TypeError.
    shouldThrow(() => pmd.toPlainDate({}), TypeError, "toPlainDate empty");
    shouldThrow(() => pmd.toPlainDate({ year: NaN }), RangeError, "toPlainDate year=NaN");

    // Feb 29 across leap/non-leap years — constrain to Feb 28 in non-leap.
    const feb29 = Temporal.PlainMonthDay.from("--02-29");
    shouldBe(feb29.toPlainDate({ year: 2024 }).toString(), "2024-02-29", "Feb29→2024 leap");
    shouldBe(feb29.toPlainDate({ year: 2023 }).toString(), "2023-02-28", "Feb29→2023 non-leap constrain");

    // Year-limit boundaries — must throw RangeError, not crash (same class as PYM Bug #2/#3).
    shouldThrow(() => pmd.toPlainDate({ year: 275761 }), RangeError, "toPlainDate year=275761");
    shouldThrow(() => pmd.toPlainDate({ year: -271822 }), RangeError, "toPlainDate year=-271822");
}

// ------------------------------------------------------------------
// toPlainDate() — era-based calendars (Japanese, Gregory)
// Regression test for a JSC bug where toPlainDate only read `year`
// and ignored `era`/`eraYear`, throwing TypeError on valid inputs.
// ------------------------------------------------------------------
{
    // Japanese Reiwa 5 = 2023 CE.
    const jpmd = Temporal.PlainMonthDay.from({ monthCode: "M03", day: 15, calendar: "japanese" });
    shouldBe(jpmd.toPlainDate({ year: 2023 }).toString(), "2023-03-15[u-ca=japanese]", "japanese year-only");
    shouldBe(jpmd.toPlainDate({ era: "reiwa", eraYear: 5 }).toString(), "2023-03-15[u-ca=japanese]", "japanese era+eraYear reiwa 5");
    shouldBe(jpmd.toPlainDate({ era: "heisei", eraYear: 30 }).toString(), "2018-03-15[u-ca=japanese]", "japanese era+eraYear heisei 30");
    // year and era+eraYear both present + consistent
    shouldBe(jpmd.toPlainDate({ year: 2023, era: "reiwa", eraYear: 5 }).toString(), "2023-03-15[u-ca=japanese]", "japanese year+era consistent");
    // year + inconsistent era+eraYear: implementation-defined (V8 lets `year` win, JSC throws).
    // Skipping the assertion since spec doesn't nail down the precedence.

    // Gregory BCE/CE.
    const gpmd = Temporal.PlainMonthDay.from({ monthCode: "M03", day: 15, calendar: "gregory" });
    shouldBe(gpmd.toPlainDate({ era: "ce", eraYear: 2024 }).toString(), "2024-03-15[u-ca=gregory]", "gregory ce+eraYear");
    shouldBe(gpmd.toPlainDate({ era: "bce", eraYear: 1 }).toString(), "0000-03-15[u-ca=gregory]", "gregory bce eraYear=1");

    // era or eraYear alone is not enough — must throw.
    shouldThrow(() => jpmd.toPlainDate({ era: "reiwa" }), TypeError, "japanese era only");
    shouldThrow(() => jpmd.toPlainDate({ eraYear: 5 }), TypeError, "japanese eraYear only");
    shouldThrow(() => jpmd.toPlainDate({}), TypeError, "japanese empty");
}

// ------------------------------------------------------------------
// toString options
// ------------------------------------------------------------------
{
    const iso = Temporal.PlainMonthDay.from("--06-15");
    shouldBe(iso.toString(), "06-15", "ISO default");
    shouldBe(iso.toString({ calendarName: "always" }), "1972-06-15[u-ca=iso8601]", "ISO always includes ref year");
    shouldBe(iso.toString({ calendarName: "critical" }), "1972-06-15[!u-ca=iso8601]", "ISO critical");
    shouldBe(iso.toString({ calendarName: "never" }), "06-15", "ISO never");

    const heb = Temporal.PlainMonthDay.from({ monthCode: "M06", day: 15, calendar: "hebrew" });
    // Non-ISO always shows the calendar annotation in default output.
    if (!heb.toString().includes("[u-ca=hebrew]"))
        throw new Error(`Hebrew default toString must include [u-ca=hebrew], got: ${heb.toString()}`);
}

// ------------------------------------------------------------------
// valueOf → TypeError
// ------------------------------------------------------------------
{
    const pmd = Temporal.PlainMonthDay.from("--06-15");
    shouldThrow(() => pmd.valueOf(), TypeError, "valueOf throws");
    shouldThrow(() => pmd < pmd, TypeError, "< coercion throws via valueOf");
}

// ------------------------------------------------------------------
// Constructor — regression tests for JSC bugs.
// Bug #8: `new PMD(6, 15, "iso8601", 275761)` was a debug-assertion
//   crash. The referenceYear was passed straight to PlainDate ctor,
//   whose assert fires on year > maxYear.
// Bug #9: Non-spec `argumentCount < 2` custom guard preempted spec's
//   `ToIntegerWithTruncation(undefined) → NaN → RangeError` path.
// ------------------------------------------------------------------
{
    // Valid — spec ctor accepts (isoMonth, isoDay, calendar?, referenceISOYear?).
    shouldBe(new Temporal.PlainMonthDay(6, 15).toString(), "06-15", "ctor(6, 15)");
    shouldBe(new Temporal.PlainMonthDay(2, 29).toString(), "02-29", "ctor(2, 29) default ref year 1972 (leap)");
    shouldBe(new Temporal.PlainMonthDay(6, 15, "iso8601", 275760).toString({ calendarName: "always" }),
        "+275760-06-15[u-ca=iso8601]", "ctor max ref year 275760");
    shouldBe(new Temporal.PlainMonthDay(6, 15, "iso8601", -271821).toString({ calendarName: "always" }),
        "-271821-06-15[u-ca=iso8601]", "ctor min ref year -271821");

    // Bug #8: out-of-range referenceYear must throw RangeError, not crash.
    shouldThrow(() => new Temporal.PlainMonthDay(6, 15, "iso8601", 275761), RangeError,
        "ctor ref year 275761 → RangeError (was: debug assert crash)");
    shouldThrow(() => new Temporal.PlainMonthDay(6, 15, "iso8601", -271822), RangeError,
        "ctor ref year -271822 → RangeError");

    // Bug #9: missing/undefined args go through ToIntegerWithTruncation → NaN → RangeError.
    shouldThrow(() => new Temporal.PlainMonthDay(), RangeError, "ctor no args → RangeError");
    shouldThrow(() => new Temporal.PlainMonthDay(6), RangeError, "ctor 1 arg (missing day) → RangeError");
    shouldThrow(() => new Temporal.PlainMonthDay(undefined, 15), RangeError, "ctor undefined month → RangeError");
    shouldThrow(() => new Temporal.PlainMonthDay(6, undefined), RangeError, "ctor undefined day → RangeError");

    // Non-finite → RangeError.
    shouldThrow(() => new Temporal.PlainMonthDay(NaN, 15), RangeError, "ctor NaN month");
    shouldThrow(() => new Temporal.PlainMonthDay(6, NaN), RangeError, "ctor NaN day");
    shouldThrow(() => new Temporal.PlainMonthDay(Infinity, 15), RangeError, "ctor Infinity month");
    shouldThrow(() => new Temporal.PlainMonthDay(6, 15, "iso8601", Infinity), RangeError, "ctor Infinity ref year");

    // Invalid dates.
    shouldThrow(() => new Temporal.PlainMonthDay(13, 15), RangeError, "ctor month 13");
    shouldThrow(() => new Temporal.PlainMonthDay(6, 31), RangeError, "ctor Jun 31");
    shouldThrow(() => new Temporal.PlainMonthDay(2, 29, "iso8601", 1971), RangeError, "ctor Feb 29 in non-leap 1971");
    shouldThrow(() => new Temporal.PlainMonthDay(0, 15), RangeError, "ctor month 0");
    shouldThrow(() => new Temporal.PlainMonthDay(6, 0), RangeError, "ctor day 0");

    // Calendar arg.
    shouldThrow(() => new Temporal.PlainMonthDay(6, 15, 42), TypeError, "ctor non-string calendar");
    shouldThrow(() => new Temporal.PlainMonthDay(6, 15, "bogus"), RangeError, "ctor bad calendar id");
    shouldBe(new Temporal.PlainMonthDay(6, 15, "hebrew").toString().includes("[u-ca=hebrew]"), true,
        "ctor with hebrew calendar preserves annotation");
}

// ------------------------------------------------------------------
// Bug #10 regression — PMD.from with {era, eraYear} on non-lunisolar
// calendars silently required monthCode when era+eraYear alone provides
// enough info to resolve the calendar month.
// ------------------------------------------------------------------
{
    shouldBe(Temporal.PlainMonthDay.from({ era: "reiwa", eraYear: 5, month: 3, day: 15, calendar: "japanese" }).toString(),
        "1972-03-15[u-ca=japanese]", "PMD japanese from era+eraYear+month+day");
    shouldBe(Temporal.PlainMonthDay.from({ era: "ce", eraYear: 2024, month: 3, day: 15, calendar: "gregory" }).toString(),
        "1972-03-15[u-ca=gregory]", "PMD gregory from era+eraYear+month+day");

    // But {month, day} without year/era must still throw — test262-mandated for non-ISO.
    shouldThrow(() => Temporal.PlainMonthDay.from({ month: 11, day: 18, calendar: "gregory" }),
        TypeError, "PMD gregory from month+day (no year/era) → TypeError");

    // Consistency check: year AND era+eraYear that disagree → RangeError.
    shouldThrow(() => Temporal.PlainMonthDay.from({ year: 2018, era: "ce", eraYear: 2024, month: 3, day: 15, calendar: "gregory" }),
        RangeError, "PMD gregory from year+era inconsistent");
}

// ------------------------------------------------------------------
// from() non-ISO — second-pass re-resolve must canonicalize the PMD
// to the ECMA reference year (1972) even when the first-pass result
// is a lunisolar leap month whose near-1972 slot is UseRegularIfConstrain.
// Regression: without the guards in monthDayFromFields' second pass,
// the sentinel refYear was passed straight through to the ICU bridge,
// which failed silently and left the PMD carrying the user's year.
// ------------------------------------------------------------------
{
    // ICU4C Chinese year 1651 has a real M01L. Guarded by pd.monthCode/day
    // in case a future ICU update changes the leap-month layout for that year.
    const pd = Temporal.PlainDate.from({ calendar: "chinese", year: 1651, monthCode: "M01L", day: 29 });
    if (pd.monthCode === "M01L" && pd.day === 29) {
        const pmd = Temporal.PlainMonthDay.from({ calendar: "chinese", year: 1651, monthCode: "M01L", day: 29 });
        const pmdYear = Number(pmd.toString().split("-")[0]);
        shouldBe(pmdYear, 1972, "PMD chinese M01L D29 uses reference year 1972");
    }
    // Reject overflow variant: leap month with no near-1972 slot must throw
    // (only when the guard input is a real leap month, else first-pass constrain
    // rewrites the flag away and this test would silently no-op).
    shouldThrow(() => Temporal.PlainMonthDay.from({ calendar: "chinese", year: 1651, monthCode: "M01L", day: 29 }, { overflow: "reject" }),
        RangeError, "PMD chinese M01L D29 reject → RangeError");
}

// ------------------------------------------------------------------
// ecmaReferenceYear edge cases — icu4x line-for-line alignment.
// icu4x reference: components/calendar/src/cal/{east_asian_traditional,hijri,hebrew,coptic,persian,indian}.rs
// ------------------------------------------------------------------

// Chinese/Dangi UseRegularIfConstrain — every icu4x-flagged leap slot.
// Constrain must drop the leap flag; Reject must throw.
{
    const trigger = [
        { monthCode: "M01L", day: 15 }, // any day → UseRegular
        { monthCode: "M01L", day: 30 },
        { monthCode: "M02L", day: 30 }, // bigDay → UseRegular
        { monthCode: "M08L", day: 30 },
        { monthCode: "M09L", day: 30 },
        { monthCode: "M10L", day: 30 },
        { monthCode: "M11L", day: 30 },
        { monthCode: "M12L", day: 15 }, // any day → UseRegular
    ];
    for (const cal of ["chinese", "dangi"]) {
        for (const { monthCode, day } of trigger) {
            const nonLeap = monthCode.slice(0, 3);
            const pmd = Temporal.PlainMonthDay.from({ calendar: cal, monthCode, day });
            shouldBe(pmd.monthCode, nonLeap, `${cal} ${monthCode} D${day} constrain drops leap flag`);
            shouldThrow(() => Temporal.PlainMonthDay.from({ calendar: cal, monthCode, day }, { overflow: "reject" }),
                RangeError, `${cal} ${monthCode} D${day} reject`);
        }
    }
}

// Chinese/Dangi non-UseRegular leap slots — leap flag preserved.
{
    const preserve = [
        { monthCode: "M02L", day: 15 },
        { monthCode: "M03L", day: 15 },
        { monthCode: "M04L", day: 15 },
        { monthCode: "M05L", day: 15 },
        { monthCode: "M06L", day: 15 },
        { monthCode: "M07L", day: 15 },
        { monthCode: "M08L", day: 15 },
        { monthCode: "M09L", day: 15 },
        { monthCode: "M10L", day: 15 },
        { monthCode: "M11L", day: 15 },
    ];
    for (const cal of ["chinese", "dangi"]) {
        for (const { monthCode, day } of preserve) {
            const pmd = Temporal.PlainMonthDay.from({ calendar: cal, monthCode, day });
            shouldBe(pmd.monthCode, monthCode, `${cal} ${monthCode} D${day} preserves leap flag`);
            shouldBe(pmd.day, day, `${cal} ${monthCode} D${day} preserves day`);
        }
    }
}

// Hebrew Cheshvan/Kislev/Tevet day-boundary: 5733 has 29 days in M02/M03 and Dec 31 = M04 D26.
{
    const boundaries = [
        { monthCode: "M02", day: 29 }, { monthCode: "M02", day: 30 },
        { monthCode: "M03", day: 29 }, { monthCode: "M03", day: 30 },
        { monthCode: "M04", day: 26 }, { monthCode: "M04", day: 27 },
    ];
    for (const { monthCode, day } of boundaries) {
        const pmd = Temporal.PlainMonthDay.from({ calendar: "hebrew", monthCode, day });
        shouldBe(pmd.monthCode, monthCode, `hebrew ${monthCode} D${day} monthCode`);
        shouldBe(pmd.day, day, `hebrew ${monthCode} D${day} day`);
    }
    // Adar I (M05L) — only valid Hebrew leap monthCode.
    const adarI = Temporal.PlainMonthDay.from({ calendar: "hebrew", monthCode: "M05L", day: 15 });
    shouldBe(adarI.monthCode, "M05L", "hebrew M05L (Adar I)");
    shouldBe(adarI.day, 15, "hebrew M05L day");
}

// Coptic / Ethiopic / Ethioaa Nasi (M13) day-6 leap-year boundary.
{
    for (const cal of ["coptic", "ethiopic", "ethioaa"]) {
        const nasi5 = Temporal.PlainMonthDay.from({ calendar: cal, monthCode: "M13", day: 5 });
        shouldBe(nasi5.monthCode, "M13", `${cal} M13 D5`);
        shouldBe(nasi5.day, 5, `${cal} M13 D5 day`);
        const nasi6 = Temporal.PlainMonthDay.from({ calendar: cal, monthCode: "M13", day: 6 });
        shouldBe(nasi6.monthCode, "M13", `${cal} M13 D6 (leap)`);
        shouldBe(nasi6.day, 6, `${cal} M13 D6 day`);
    }
}

// Persian / Indian M10 day-10 boundary.
{
    for (const cal of ["persian", "indian"]) {
        const inYear = Temporal.PlainMonthDay.from({ calendar: cal, monthCode: "M10", day: 10 });
        shouldBe(inYear.day, 10, `${cal} M10 D10`);
        const nextDay = Temporal.PlainMonthDay.from({ calendar: cal, monthCode: "M10", day: 11 });
        shouldBe(nextDay.day, 11, `${cal} M10 D11`);
    }
}

// Islamic UmmAlQura big-day fallback + civil/tbla M11 epoch difference.
{
    const cases = [
        { monthCode: "M02", day: 30 },
        { monthCode: "M03", day: 30 },
        { monthCode: "M05", day: 30 },
        { monthCode: "M07", day: 30 },
        { monthCode: "M10", day: 30 },
        { monthCode: "M11", day: 26 },
        { monthCode: "M12", day: 30 },
        { monthCode: "M12", day: 29 },
    ];
    for (const { monthCode, day } of cases) {
        const pmd = Temporal.PlainMonthDay.from({ calendar: "islamic-umalqura", monthCode, day });
        shouldBe(pmd.monthCode, monthCode, `islamic-umalqura ${monthCode} D${day}`);
        shouldBe(pmd.day, day, `islamic-umalqura ${monthCode} D${day} day`);
    }
    // civil = Friday epoch, day<26 boundary; tbla = Thursday epoch, day<27 boundary.
    shouldBe(Temporal.PlainMonthDay.from({ calendar: "islamic-civil", monthCode: "M11", day: 25 }).day, 25);
    shouldBe(Temporal.PlainMonthDay.from({ calendar: "islamic-civil", monthCode: "M11", day: 26 }).day, 26);
    shouldBe(Temporal.PlainMonthDay.from({ calendar: "islamic-tbla",  monthCode: "M11", day: 26 }).day, 26);
    shouldBe(Temporal.PlainMonthDay.from({ calendar: "islamic-tbla",  monthCode: "M11", day: 27 }).day, 27);
}

// MonthNotInCalendar rejections.
{
    for (const cal of ["coptic", "ethiopic", "ethioaa", "persian", "indian", "islamic-umalqura", "islamic-civil", "islamic-tbla"]) {
        shouldThrow(() => Temporal.PlainMonthDay.from({ calendar: cal, monthCode: "M02L", day: 15 }),
            RangeError, `${cal} rejects M02L`);
        shouldThrow(() => Temporal.PlainMonthDay.from({ calendar: cal, monthCode: "M05L", day: 15 }),
            RangeError, `${cal} rejects M05L`);
    }
    // Hebrew: only M05L (Adar I) is valid.
    for (const mc of ["M01L", "M02L", "M03L", "M04L", "M06L", "M07L", "M08L", "M09L", "M10L", "M11L", "M12L"]) {
        shouldThrow(() => Temporal.PlainMonthDay.from({ calendar: "hebrew", monthCode: mc, day: 15 }),
            RangeError, `hebrew rejects ${mc}`);
    }
}
