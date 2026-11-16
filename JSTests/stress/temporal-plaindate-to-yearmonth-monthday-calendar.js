//@ requireOptions("--useTemporal=1")

// PlainDate.toPlainYearMonth()/toPlainMonthDay() calendar propagation.
// Calendar propagation for non-ISO types must be preserved across conversions.

function assert(cond, msg) {
    if (!cond) throw new Error("FAIL: " + msg);
}

// Hebrew PlainDate: 1 Tishri 5784 = 2023-09-16 ISO
const pd = Temporal.PlainDate.from({ year: 5784, month: 1, day: 1, calendar: "hebrew" });
assert(pd.calendarId === "hebrew", "calendarId should be hebrew");

// toPlainYearMonth: CalendarYearMonthFromFields should canonicalize the ISO reference
// date to the first day of that Hebrew month (1 Tishri 5784 = 2023-09-16).
const pym = pd.toPlainYearMonth();
assert(pym.calendarId === "hebrew",
    "toPlainYearMonth calendarId should be hebrew, got: " + pym.calendarId);
// The PYM's reference ISO date should be 2023-09-16 (start of Tishri 5784), not
// (5784, 1, 1) in raw ISO which would be a wildly wrong date.
const pymBack = pym.toPlainDate({ day: 1 });
assert(pymBack.calendarId === "hebrew", "round-trip calendarId should be hebrew");
assert(pymBack.year === 5784, "round-trip year should be 5784, got: " + pymBack.year);
assert(pymBack.month === 1, "round-trip month should be 1, got: " + pymBack.month);

// toPlainMonthDay: CalendarMonthDayFromFields should pick the correct reference ISO year.
const pmd = pd.toPlainMonthDay();
assert(pmd.calendarId === "hebrew",
    "toPlainMonthDay calendarId should be hebrew, got: " + pmd.calendarId);
// Round-trip: PMD.toPlainDate with year 5784 should give back 1 Tishri 5784.
const pmdBack = pmd.toPlainDate({ year: 5784 });
assert(pmdBack.calendarId === "hebrew", "round-trip calendarId should be hebrew");
assert(pmdBack.year === 5784, "round-trip year should be 5784, got: " + pmdBack.year);
assert(pmdBack.month === 1, "round-trip month should be 1, got: " + pmdBack.month);
assert(pmdBack.day === 1, "round-trip day should be 1, got: " + pmdBack.day);
