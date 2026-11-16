//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Helper: assert diff equals ISO diff (non-ISO calendars use pure ISO arithmetic)
function assertMatchesISO(isoOneStr, isoTwoStr, largestUnit, calendars) {
    const isoOne = Temporal.PlainDate.from(isoOneStr);
    const isoTwo = Temporal.PlainDate.from(isoTwoStr);
    const isoRef = isoOne.until(isoTwo, { largestUnit });
    for (const cal of calendars) {
        const one = isoOne.withCalendar(cal);
        const two = isoTwo.withCalendar(cal);
        const diff = one.until(two, { largestUnit });
        const msg = `${cal} ${isoOneStr} until ${isoTwoStr} ${largestUnit}`;
        if (diff.years !== isoRef.years || diff.months !== isoRef.months || diff.days !== isoRef.days)
            throw new Error(`${msg}: expected ${isoRef} got ${diff}`);
        // Round-trip: one.add(diff) == two
        const roundTrip = one.add(diff);
        if (!roundTrip.equals(two))
            throw new Error(`${msg} round-trip: ${one}.add(${diff}) = ${roundTrip} ≠ ${two}`);
    }
}

const CALS = ["gregory", "buddhist", "japanese", "roc"];

// ── Bug 1: Incremental month clamping ──────────────────────────────────────
// Mar 31 -1mo → Feb 29 (clamp) -1mo → Jan 29 — WRONG
// Fix: single ucal_add, Mar 31 -2mo → Jan 31 — CORRECT

assertMatchesISO("2020-03-31", "2020-01-01", "months", CALS);
assertMatchesISO("2020-01-01", "2020-03-31", "months", CALS);

// Month-end clamp: last day of long month → last day of short month
assertMatchesISO("2020-01-31", "2020-02-29", "months", CALS); // Jan 31 → Feb 29 (leap)
assertMatchesISO("2020-01-31", "2020-04-30", "months", CALS); // Jan 31 → Apr 30
assertMatchesISO("2020-03-31", "2020-06-30", "months", CALS); // Mar 31 → Jun 30

// Negative direction
assertMatchesISO("2020-03-31", "2020-01-31", "months", CALS); // -2M0D
assertMatchesISO("2020-04-30", "2020-01-31", "months", CALS); // -3M0D
assertMatchesISO("2020-06-30", "2020-03-31", "months", CALS); // -3M0D

// ── Bug 2: Undo asymmetry (ucal_add +1mo then -1mo ≠ identity at month-end) ─
// Jan 29 + 1mo = Feb 28 (2100 not leap), Feb 28 - 1mo = Jan 28 — WRONG
// Fix: single add, same result

assertMatchesISO("2020-01-29", "2100-01-29", "years", CALS);  // P80Y exactly
assertMatchesISO("2020-01-31", "2100-01-31", "years", CALS);  // P80Y exactly
assertMatchesISO("2020-02-29", "2100-01-01", "years", CALS);  // P79Y10M3D (source day=29)

// ── Bug 3: Source day preservation (use original day, not clamped year-advance day) ─
// Feb 29 + 79 years = Feb 28, 2099. Source day=29, target-month Dec has 31 days → Dec 29
assertMatchesISO("2020-02-29", "2099-12-01", "years", CALS);
assertMatchesISO("2020-02-29", "2100-03-01", "years", CALS);

// ── Bug 4: Julian/Gregorian reform (ICU 'gregory' uses Julian before 1582-10-15) ──
// Pure ISO arithmetic avoids the 10-day calendar reform gap
assertMatchesISO("1581-01-01", "2020-01-01", "years", CALS);  // P439Y exactly
assertMatchesISO("1581-01-01", "2020-01-01", "months", CALS); // P5268M exactly
assertMatchesISO("1582-10-01", "2020-01-01", "years", CALS);  // spans reform date
assertMatchesISO("1582-10-15", "2020-01-01", "years", CALS);  // reform date itself
assertMatchesISO("1581-12-31", "1582-01-01", "months", CALS); // 1-day difference across reform year

// ── Extreme boundary years ─────────────────────────────────────────────────
// Large year spans exercise the min_years fast-forward optimization
assertMatchesISO("-271821-04-19", "+275760-09-13", "years", ["gregory"]);  // full Temporal range
assertMatchesISO("1970-01-01", "+275760-09-13", "years", CALS);
assertMatchesISO("-271821-04-19", "1970-01-01", "years", CALS);
assertMatchesISO("1900-01-01", "2100-12-31", "months", CALS); // 2400 months

// ── Epoch and negative years ───────────────────────────────────────────────
assertMatchesISO("1970-01-01", "2020-01-01", "years", CALS);  // P50Y
assertMatchesISO("1969-12-31", "2020-01-01", "years", CALS);  // P50Y1D
assertMatchesISO("0000-01-01", "2020-01-01", "years", CALS);  // year 0
assertMatchesISO("-000001-01-01", "2020-01-01", "years", CALS); // negative year

// ── Month-day combinations from benchmark interestingMonthDays ────────────
// (subset of the cross-product that exercises boundary conditions)
const edgePairs = [
    ["2016-01-29", "2020-02-29"],  // leap day in target
    ["2016-02-29", "2020-02-29"],  // leap day in both
    ["2020-02-29", "2020-03-01"],  // day after leap day
    ["2020-01-31", "2020-03-01"],  // Jan 31 + 1mo = Mar 1 (NOT Feb 31)
    ["2019-02-28", "2020-02-29"],  // non-leap to leap Feb
    ["2020-02-29", "2021-02-28"],  // leap to non-leap Feb
    ["2020-03-29", "2020-04-19"],  // within quarter
    ["2020-06-30", "2020-07-31"],  // short to long month
    ["2020-07-31", "2020-06-30"],  // long to short (negative)
    ["2020-09-13", "+275760-09-13"],  // last supported monthday
    ["2020-04-19", "2020-04-19"],  // same date → zero duration
];
for (const [a, b] of edgePairs) {
    assertMatchesISO(a, b, "months", CALS);
    assertMatchesISO(a, b, "years", CALS);
}

// ── since() symmetry ───────────────────────────────────────────────────────
// one.since(two) must equal two.until(one) for all calendars
for (const cal of CALS) {
    const pairs = [
        ["2020-03-31", "2020-01-01"],
        ["2020-01-29", "2100-01-29"],
        ["2016-02-29", "2020-02-29"],
    ];
    for (const [a, b] of pairs) {
        const one = Temporal.PlainDate.from(a).withCalendar(cal);
        const two = Temporal.PlainDate.from(b).withCalendar(cal);
        const until = one.until(two, { largestUnit: "months" });
        const since = two.since(one, { largestUnit: "months" });
        shouldBe(until.toString(), since.toString());
    }
}

// ── largestUnit: "years" vs "months" consistency ───────────────────────────
// years result and months result must be consistent: years*12 + months_remainder = total_months
for (const cal of CALS) {
    const one = Temporal.PlainDate.from("2020-03-31").withCalendar(cal);
    const two = Temporal.PlainDate.from("2023-01-15").withCalendar(cal);
    const byYears = one.until(two, { largestUnit: "years" });
    const byMonths = one.until(two, { largestUnit: "months" });
    shouldBe(byYears.years * 12 + byYears.months, byMonths.months);
    shouldBe(byYears.days, byMonths.days);
}
