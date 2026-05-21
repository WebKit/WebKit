//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Bug: calendarDateUntil for Chinese/Dangi (lunisolar) calendars with largestUnit='days'
// or 'weeks' used ICU epoch-ms arithmetic. ICU's Chinese calendar approximation for
// dates far beyond ~year 2100 gives wrong epoch ms, causing incorrect day counts.
// e.g. 2020-01-01 until +275760-01-01 in Chinese calendar: expected 99981482 days,
// got 99999744 days (18262 extra ≈ 50 years).
//
// Fix: like ICU4X's fast path (calendar_arithmetic.rs:934-958), use pure ISO day count
// (diffISODate) for days/weeks regardless of calendar — day count is calendar-independent.

const ISO_ONE = Temporal.PlainDate.from("2020-01-01");
const ISO_TWO = Temporal.PlainDate.from("+275760-01-01");

// ISO reference
const isoWeeks = ISO_ONE.until(ISO_TWO, { largestUnit: "weeks" });
const isoDays  = ISO_ONE.until(ISO_TWO, { largestUnit: "days" });
shouldBe(isoWeeks.weeks * 7 + isoWeeks.days, 99981482);
shouldBe(isoDays.days, 99981482);

// All calendars must match ISO for days/weeks (calendar doesn't affect raw day count)
for (const cal of ["chinese", "dangi", "hebrew", "islamic-umalqura", "coptic", "persian", "ethiopic", "buddhist", "gregory"]) {
    const d1 = ISO_ONE.withCalendar(cal);
    const d2 = ISO_TWO.withCalendar(cal);

    const w = d1.until(d2, { largestUnit: "weeks" });
    shouldBe(w.weeks * 7 + w.days, 99981482);  // must match ISO raw day count

    const d = d1.until(d2, { largestUnit: "days" });
    shouldBe(d.days, 99981482);  // must match ISO raw day count
}

// Normal date span (well within ICU's accurate range)
const near1 = Temporal.PlainDate.from("2020-01-01");
const near2 = Temporal.PlainDate.from("2023-06-15");
for (const cal of ["chinese", "dangi", "hebrew", "islamic-umalqura"]) {
    const d1 = near1.withCalendar(cal);
    const d2 = near2.withCalendar(cal);
    const isoRef = near1.until(near2, { largestUnit: "days" });
    const calDiff = d1.until(d2, { largestUnit: "days" });
    shouldBe(calDiff.days, isoRef.days);
}

// Negative direction (since)
for (const cal of ["chinese", "dangi"]) {
    const d1 = ISO_ONE.withCalendar(cal);
    const d2 = ISO_TWO.withCalendar(cal);
    const s = d2.since(d1, { largestUnit: "days" });
    shouldBe(s.days, 99981482);
}
