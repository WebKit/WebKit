//@ requireOptions("--useTemporal=1")

// PlainMonthDay.toPlainDate() calendar propagation for non-ISO types.
// Calendar propagation for non-ISO types must be preserved across conversions.

function assert(cond, msg) {
    if (!cond) throw new Error("FAIL: " + msg);
}

// Create a Hebrew PlainMonthDay: month 1 (Tishri), day 1.
// The PMD's reference ISO date should be 2023-09-16 (1 Tishri 5784 in a non-leap year).
const pmd = Temporal.PlainMonthDay.from({ monthCode: "M01", day: 1, calendar: "hebrew" });
assert(pmd.calendarId === "hebrew", "calendarId should be hebrew");

// toPlainDate with year 5784: should give 1 Tishri 5784 = 2023-09-16.
const pd = pmd.toPlainDate({ year: 5784 });
assert(pd.calendarId === "hebrew",
    "toPlainDate calendarId should be hebrew, got: " + pd.calendarId);
assert(pd.year === 5784, "year should be 5784, got: " + pd.year);
assert(pd.month === 1, "month should be 1 (Tishri), got: " + pd.month);
assert(pd.day === 1, "day should be 1, got: " + pd.day);

// Verify the ISO date is correct: 1 Tishri 5784 = 2023-09-16.
// If JSC uses raw regulateISODate it would produce a wrong ISO date.
assert(pd.toPlainDateTime().toString().startsWith("2023-09-16"),
    "ISO date of 1 Tishri 5784 should be 2023-09-16, got: " + pd.toPlainDateTime().toString());

// toPlainDate with year 5785 (next year): should give 1 Tishri 5785 = 2024-10-03.
const pd2 = pmd.toPlainDate({ year: 5785 });
assert(pd2.year === 5785, "year should be 5785, got: " + pd2.year);
assert(pd2.month === 1, "month should be 1, got: " + pd2.month);
assert(pd2.toPlainDateTime().toString().startsWith("2024-10-03"),
    "ISO date of 1 Tishri 5785 should be 2024-10-03, got: " + pd2.toPlainDateTime().toString());
