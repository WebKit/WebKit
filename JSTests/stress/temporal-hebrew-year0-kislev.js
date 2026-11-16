//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

const kislev30 = Temporal.PlainDate.from({calendar: "hebrew", year: 0, monthCode: "M03", day: 30});
shouldBe(kislev30.day, 30, "y0 Kislev D30 .day");
shouldBe(kislev30.monthCode, "M03", "y0 Kislev D30 .monthCode");
shouldBe(kislev30.year, 0, "y0 Kislev D30 .year");

// day <= daysInMonth is a Temporal invariant. Pre-fix: daysInMonth=29.
shouldBe(kislev30.daysInMonth, 30, "y0 Kislev D30 .daysInMonth (pre-fix: 29)");
if (!(kislev30.day <= kislev30.daysInMonth))
    throw new Error("Invariant day <= daysInMonth violated");

// y0 is Regular leap (384 days) per icu4x, not Deficient leap (383).
const tishri1 = Temporal.PlainDate.from({calendar: "hebrew", year: 0, monthCode: "M01", day: 1});
shouldBe(tishri1.daysInYear, 384, "y0 .daysInYear (pre-fix: 383)");
shouldBe(tishri1.inLeapYear, true, "y0 .inLeapYear");
shouldBe(tishri1.monthsInYear, 13, "y0 .monthsInYear");

// PlainDateTime and PlainYearMonth share the accessor implementations.
const pdt = Temporal.PlainDateTime.from({calendar: "hebrew", year: 0, monthCode: "M03", day: 30});
shouldBe(pdt.daysInMonth, 30, "PDT y0 Kislev D30 .daysInMonth");
shouldBe(pdt.daysInYear, 384, "PDT y0 .daysInYear");

const pym = Temporal.PlainYearMonth.from({calendar: "hebrew", year: 0, monthCode: "M03"});
shouldBe(pym.daysInMonth, 30, "PYM y0 Kislev .daysInMonth");
shouldBe(pym.daysInYear, 384, "PYM y0 .daysInYear");

// .add({months:1}) from Kislev D30 must land on Tevet (M04), not Shevat (M05).
// Pre-fix: origMonthCode snapshot read ICU's "M04" (Tevet slot) and ucal_add advanced from
// Tevet to Shevat, dropping a month.
const plusOne = kislev30.add({months: 1});
shouldBe(plusOne.monthCode, "M04", "y0 Kislev D30 + 1mo .monthCode (pre-fix: M05)");
shouldBe(plusOne.year, 0, "y0 Kislev D30 + 1mo .year stays 0");
// Tevet has 29 days per both ICU and icu4x; day 30 constrains to 29.
shouldBe(plusOne.day, 29, "y0 Kislev D30 + 1mo .day (Tevet has 29)");

// .add({years:1}) from Kislev D30 preserves monthCode M03 in the target year.
const plusOneYear = kislev30.add({years: 1});
shouldBe(plusOneYear.year, 1, "y0 Kislev D30 + 1y .year");
shouldBe(plusOneYear.monthCode, "M03", "y0 Kislev D30 + 1y .monthCode (pre-fix: M04)");

// .subtract({months:1}) from Kislev D30 = Cheshvan (M02).
const minusOne = kislev30.subtract({months: 1});
shouldBe(minusOne.monthCode, "M02", "y0 Kislev D30 - 1mo .monthCode");

// Sanity: y0 M03 D29 (non-fabricated Kislev) is unaffected — normal ICU path.
const kislev29 = Temporal.PlainDate.from({calendar: "hebrew", year: 0, monthCode: "M03", day: 29});
shouldBe(kislev29.day, 29, "y0 Kislev D29 .day");
shouldBe(kislev29.monthCode, "M03", "y0 Kislev D29 .monthCode");
shouldBe(kislev29.add({months: 1}).monthCode, "M04", "y0 Kislev D29 + 1mo .monthCode");
shouldBe(kislev29.add({months: 1}).day, 29, "y0 Kislev D29 + 1mo .day");

// Sanity: post-y0 (year 1) Hebrew years are unaffected by the workaround.
const y1kislev = Temporal.PlainDate.from({calendar: "hebrew", year: 1, monthCode: "M03", day: 1});
shouldBe(y1kislev.daysInYear >= 353 && y1kislev.daysInYear <= 385, true, "y1 daysInYear in valid range");

// Ordinal-month and monthCode input must agree on whether y0 Kislev day 30 exists: the maxDay
// override is keyed on calendar position (year 0, Kislev), not on which field selected the month.
{
    const viaMonthCode = Temporal.PlainDate.from({calendar: "hebrew", year: 0, monthCode: "M03", day: 30});
    const viaOrdinal = Temporal.PlainDate.from({calendar: "hebrew", year: 0, month: 3, day: 30}, {overflow: "constrain"});
    shouldBe(viaOrdinal.day, viaMonthCode.day, "y0 ordinal month=3 day=30 must agree with monthCode M03 day=30");

    let threw = false;
    try {
        Temporal.PlainDate.from({calendar: "hebrew", year: 0, month: 3, day: 30}, {overflow: "reject"});
    } catch (e) {
        threw = e instanceof RangeError;
    }
    shouldBe(threw, false, "y0 ordinal month=3 day=30 overflow:reject must not throw");
}

// FIXME: rdar://182958553 (icu-issues/01) - the Kislev D30 fabrication only relabels the single
// Tevet D1 slot, so it doesn't shift the rest of the year. Wait for ICU's classification to
// match icu4x rather than growing more relabeling logic; flip to `if (true)` once fixed.
if (false) {
    // The last day of the year must have dayOfYear === daysInYear.
    {
        const elul29 = Temporal.PlainDate.from({calendar: "hebrew", year: 0, monthCode: "M12", day: 29});
        shouldBe(elul29.daysInYear, elul29.dayOfYear, "y0 last day (Elul 29): dayOfYear must equal daysInYear (currently 383 vs 384)");
    }

    // "Tevet D1" is currently unreachable: monthCode input collapses it to Kislev D30, and
    // arithmetic (+1 day from Kislev D30) skips straight to Tevet D2.
    {
        const requestedTevet1 = Temporal.PlainDate.from({calendar: "hebrew", year: 0, monthCode: "M04", day: 1});
        shouldBe(requestedTevet1.monthCode, "M04", "y0 requested Tevet D1 .monthCode (currently collapses to M03)");
        shouldBe(requestedTevet1.day, 1, "y0 requested Tevet D1 .day (currently collapses to 30)");

        const dayAfterKislev30 = kislev30.add({days: 1});
        shouldBe(dayAfterKislev30.monthCode, "M04", "y0 Kislev D30 + 1 day .monthCode");
        shouldBe(dayAfterKislev30.day, 1, "y0 Kislev D30 + 1 day .day (currently skips to 2)");
    }

    // PlainYearMonth and PlainDate must agree on Tevet's daysInMonth in y0.
    {
        const pymTevet = Temporal.PlainYearMonth.from({calendar: "hebrew", year: 0, monthCode: "M04"});
        const pdTevet = Temporal.PlainDate.from({calendar: "hebrew", year: 0, monthCode: "M04", day: 15});
        shouldBe(pymTevet.daysInMonth, pdTevet.daysInMonth, "y0 Tevet .daysInMonth must agree between PlainYearMonth (currently 30) and PlainDate (29)");
    }
}
