//@ requireOptions("--useTemporal=1")

// PlainDateTime calendar propagation for non-ISO types across conversions.
// Calendar propagation for non-ISO types must be preserved across conversions.

function assert(cond, msg) {
    if (!cond) throw new Error("FAIL: " + msg);
}

// 1. PlainDate with non-ISO calendar -> toPlainDateTime must keep the calendar.
const pd = Temporal.PlainDate.from({ year: 5784, month: 7, day: 1, calendar: "hebrew" });
assert(pd.calendarId === "hebrew", "PlainDate calendarId should be hebrew");

const pdt = pd.toPlainDateTime();
assert(pdt.calendarId === "hebrew",
    "PlainDateTime from PlainDate must preserve calendarId, got: " + pdt.calendarId);

// 2. Temporal.PlainDateTime.from(hebrewPlainDate) must preserve calendar.
const pdt2 = Temporal.PlainDateTime.from(pd);
assert(pdt2.calendarId === "hebrew",
    "PlainDateTime.from(PlainDate) must preserve calendarId, got: " + pdt2.calendarId);

// 3. ZonedDateTime with non-ISO calendar -> toPlainDateTime must keep the calendar.
const zdt = Temporal.ZonedDateTime.from("2024-03-15T12:00:00[UTC][u-ca=hebrew]");
assert(zdt.calendarId === "hebrew", "ZonedDateTime calendarId should be hebrew");
const pdtFromZdt = zdt.toPlainDateTime();
assert(pdtFromZdt.calendarId === "hebrew", "PlainDateTime from ZDT must preserve calendarId");
