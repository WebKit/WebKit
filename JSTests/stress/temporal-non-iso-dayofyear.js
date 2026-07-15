//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected}, got ${actual}`);
}

function assertFirstTwoDays(create, name) {
    const date = create();
    shouldBe(date.dayOfYear, 1, `${name} first day`);
    shouldBe(date.add({ days: 1 }).dayOfYear, 2, `${name} second day`);
}

assertFirstTwoDays(() => Temporal.PlainDate.from({ year: 5730, month: 1, day: 1, calendar: "hebrew" }), "Hebrew PlainDate");
assertFirstTwoDays(() => Temporal.PlainDateTime.from({ year: 5730, month: 1, day: 1, calendar: "hebrew", hour: 12 }), "Hebrew PlainDateTime");
assertFirstTwoDays(() => Temporal.ZonedDateTime.from({ year: 5730, month: 1, day: 1, calendar: "hebrew", hour: 12, timeZone: "UTC" }), "Hebrew ZonedDateTime");

assertFirstTwoDays(() => Temporal.PlainDate.from({ year: 1969, month: 1, day: 1, calendar: "chinese" }), "Chinese PlainDate");
assertFirstTwoDays(() => Temporal.PlainDate.from({ year: 1969, month: 1, day: 1, calendar: "dangi" }), "Dangi PlainDate");

function assertCalendarDayOfYear(date, expected, name) {
    shouldBe(date.dayOfYear, expected, `${name} day of year`);
}

assertCalendarDayOfYear(Temporal.PlainDate.from("1582-01-01").withCalendar("japanese"), 1, "Japanese PlainDate before 1873");
assertCalendarDayOfYear(Temporal.PlainDateTime.from("1582-01-01T12:00").withCalendar("japanese"), 1, "Japanese PlainDateTime before 1873");
assertCalendarDayOfYear(Temporal.ZonedDateTime.from("1582-01-01T12:00Z[UTC]").withCalendar("japanese"), 1, "Japanese ZonedDateTime before 1873");

assertCalendarDayOfYear(Temporal.PlainDate.from("1872-12-31").withCalendar("japanese"), 366, "Japanese PlainDate fallback boundary");
assertCalendarDayOfYear(Temporal.PlainDateTime.from("1872-12-31T12:00").withCalendar("japanese"), 366, "Japanese PlainDateTime fallback boundary");
assertCalendarDayOfYear(Temporal.ZonedDateTime.from("1872-12-31T12:00Z[UTC]").withCalendar("japanese"), 366, "Japanese ZonedDateTime fallback boundary");
assertCalendarDayOfYear(Temporal.PlainDate.from("1873-01-01").withCalendar("japanese"), 1, "Japanese PlainDate ICU boundary");
assertCalendarDayOfYear(Temporal.PlainDateTime.from("1873-01-01T12:00").withCalendar("japanese"), 1, "Japanese PlainDateTime ICU boundary");
assertCalendarDayOfYear(Temporal.ZonedDateTime.from("1873-01-01T12:00Z[UTC]").withCalendar("japanese"), 1, "Japanese ZonedDateTime ICU boundary");

assertCalendarDayOfYear(Temporal.PlainDate.from("1582-01-01").withCalendar("roc"), 1, "ROC PlainDate before 1582");
assertCalendarDayOfYear(Temporal.PlainDateTime.from("1582-01-01T12:00").withCalendar("roc"), 1, "ROC PlainDateTime before 1582");
assertCalendarDayOfYear(Temporal.ZonedDateTime.from("1582-01-01T12:00Z[UTC]").withCalendar("roc"), 1, "ROC ZonedDateTime before 1582");
assertCalendarDayOfYear(Temporal.PlainDate.from("1582-01-01").withCalendar("buddhist"), 1, "Buddhist PlainDate before 1582");
assertCalendarDayOfYear(Temporal.PlainDateTime.from("1582-01-01T12:00").withCalendar("buddhist"), 1, "Buddhist PlainDateTime before 1582");
assertCalendarDayOfYear(Temporal.ZonedDateTime.from("1582-01-01T12:00Z[UTC]").withCalendar("buddhist"), 1, "Buddhist ZonedDateTime before 1582");
