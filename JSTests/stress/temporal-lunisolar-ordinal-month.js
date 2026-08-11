//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected}, got ${actual}`);
}

function shouldThrow(errorType, callback, message) {
    try {
        callback();
    } catch (error) {
        if (error instanceof errorType)
            return;
        throw new Error(`${message}: expected ${errorType.name}, got ${error}`);
    }
    throw new Error(`${message}: expected ${errorType.name}`);
}

for (const { calendar, year, ordinal, monthCode } of [
    { calendar: "chinese", year: 2012, ordinal: 5, monthCode: "M04L" },
    { calendar: "dangi", year: 2012, ordinal: 4, monthCode: "M03L" },
]) {
    const fields = { calendar, year, month: ordinal, day: 1 };
    const coded = Temporal.PlainDate.from({ calendar, year, monthCode, day: 1 });
    for (const { name, create } of [
        { name: "PlainDate", create: () => Temporal.PlainDate.from(fields) },
        { name: "PlainDateTime", create: () => Temporal.PlainDateTime.from({ ...fields, hour: 12 }) },
        { name: "PlainYearMonth", create: () => Temporal.PlainYearMonth.from(fields) },
        { name: "ZonedDateTime", create: () => Temporal.ZonedDateTime.from({ ...fields, hour: 12, timeZone: "UTC" }) },
    ]) {
        const result = create();
        shouldBe(result.month, ordinal, `${name} ${calendar} ordinal month`);
        shouldBe(result.monthCode, monthCode, `${name} ${calendar} leap month code`);
    }
    shouldBe(Temporal.PlainDate.from(fields).equals(coded), true, `${calendar} numeric/coded equality`);
    const previous = Temporal.PlainDate.from({ calendar, year, month: ordinal - 1, day: 1 });
    shouldBe(previous.with({ month: ordinal }).monthCode, monthCode, `${calendar} PlainDate.with`);
    shouldBe(Temporal.PlainDateTime.from({ ...fields, month: ordinal - 1 }).with({ month: ordinal }).monthCode, monthCode, `${calendar} PlainDateTime.with`);
    shouldBe(Temporal.PlainYearMonth.from({ ...fields, month: ordinal - 1 }).with({ month: ordinal }).monthCode, monthCode, `${calendar} PlainYearMonth.with`);
    shouldBe(Temporal.ZonedDateTime.from({ ...fields, month: ordinal - 1, timeZone: "UTC" }).with({ month: ordinal }).monthCode, monthCode, `${calendar} ZonedDateTime.with`);
    shouldThrow(RangeError, () => Temporal.PlainDate.from({ ...fields, month: ordinal + 1, monthCode }), `${calendar} month/monthCode consistency`);
    shouldThrow(TypeError, () => Temporal.PlainMonthDay.from({ calendar, month: ordinal, monthCode, day: 1 }), `${calendar} PlainMonthDay.from year requirement`);
    shouldBe(Temporal.PlainMonthDay.from({ ...fields, monthCode }).monthCode, monthCode, `${calendar} PlainMonthDay.from with year`);
    const monthDay = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 1 });
    shouldThrow(TypeError, () => monthDay.with({ month: ordinal, monthCode }), `${calendar} PlainMonthDay.with year requirement`);
    shouldBe(monthDay.with({ year, month: ordinal, monthCode }).monthCode, monthCode, `${calendar} PlainMonthDay.with with year`);
}

for (const { calendar, isoDate, year, month, monthCode } of [
    { calendar: "chinese", isoDate: "2012-05-21", year: 2012, month: 5, monthCode: "M04L" },
    { calendar: "dangi", isoDate: "2020-05-23", year: 2020, month: 5, monthCode: "M04L" },
]) {
    const date = Temporal.PlainDate.from(`${isoDate}[u-ca=${calendar}]`);
    shouldBe(date.year, year, `${calendar} related year`);
    shouldBe(date.month, month, `${calendar} extracted ordinal month`);
    shouldBe(date.monthCode, monthCode, `${calendar} extracted month code`);
}

for (const [calendar, monthCode, oldMonth, newMonth] of [["chinese", "M06L", 7, 6], ["dangi", "M05L", 6, 5]]) {
    const date = Temporal.PlainDate.from({ calendar, year: 2017, monthCode, day: 1 });
    shouldBe(date.month, oldMonth, `${calendar} source leap ordinal`);
    shouldBe(date.with({ year: 2024 }).month, newMonth, `${calendar} with year inherits monthCode only`);
}

for (let i = 0; i < 100; ++i)
    shouldBe(Temporal.PlainDate.from({ calendar: "chinese", year: 2012, month: 5, day: 1 }).monthCode, "M04L", "JIT ordinal month");

for (const relativeTo of [
    { calendar: "chinese", year: 2012, month: 5, monthCode: "M04L", day: 1 },
    { calendar: "dangi", year: 2012, month: 4, monthCode: "M03L", day: 1 },
    { calendar: "hebrew", year: 5784, month: 7, monthCode: "M06", day: 1 },
]) {
    const monthCodeOnly = { ...relativeTo };
    delete monthCodeOnly.month;
    const duration = Temporal.Duration.from({ months: 1, days: 15 });
    const mismatch = { ...relativeTo, month: relativeTo.month + 1 };
    shouldBe(Temporal.Duration.compare({ months: 1 }, { days: 29 }, { relativeTo }), Temporal.Duration.compare({ months: 1 }, { days: 29 }, { relativeTo: monthCodeOnly }), `${relativeTo.calendar} Duration.compare relativeTo ordinal month`);
    shouldBe(duration.round({ largestUnit: "days", relativeTo }).toString(), duration.round({ largestUnit: "days", relativeTo: monthCodeOnly }).toString(), `${relativeTo.calendar} Duration.round relativeTo ordinal month`);
    shouldBe(duration.total({ unit: "days", relativeTo }), duration.total({ unit: "days", relativeTo: monthCodeOnly }), `${relativeTo.calendar} Duration.total relativeTo ordinal month`);
    shouldThrow(RangeError, () => Temporal.Duration.compare({ months: 1 }, { days: 29 }, { relativeTo: mismatch }), `${relativeTo.calendar} Duration.compare rejects ordinal mismatch`);
    shouldThrow(RangeError, () => duration.round({ largestUnit: "days", relativeTo: mismatch }), `${relativeTo.calendar} Duration.round rejects ordinal mismatch`);
    shouldThrow(RangeError, () => duration.total({ unit: "days", relativeTo: mismatch }), `${relativeTo.calendar} Duration.total rejects ordinal mismatch`);
}

for (const { calendar, year, month, monthCode } of [
    { calendar: "gregory", year: 2021, month: 4, monthCode: "M04" },
    { calendar: "hebrew", year: 5784, month: 7, monthCode: "M06" },
    { calendar: "islamic-civil", year: 1445, month: 4, monthCode: "M04" },
]) {
    shouldThrow(TypeError, () => Temporal.PlainMonthDay.from({ calendar, month, monthCode, day: 1 }), `${calendar} PlainMonthDay.from year requirement`);
    shouldThrow(TypeError, () => Temporal.PlainMonthDay.from({ calendar, year, day: 32 }, { overflow: "reject" }), `${calendar} missing month before day range`);
    shouldThrow(TypeError, () => Temporal.PlainMonthDay.from({ calendar, year, month, monthCode: "M01" }), `${calendar} missing day before month conflict`);
    shouldThrow(TypeError, () => Temporal.PlainMonthDay.from({ calendar, year: 300001, monthCode }), `${calendar} missing day before year range`);
    shouldThrow(TypeError, () => Temporal.PlainMonthDay.from({ calendar, year: 300001, day: 1 }), `${calendar} missing month before year range`);
    const monthDay = Temporal.PlainMonthDay.from({ calendar, monthCode, day: 1 });
    shouldBe(monthDay.monthCode, monthCode, `${calendar} monthCode-only reference year`);
    shouldThrow(TypeError, () => monthDay.with({ month, monthCode }), `${calendar} PlainMonthDay.with year requirement`);
    shouldBe(monthDay.with({ year, month, monthCode }).monthCode, monthCode, `${calendar} PlainMonthDay.with with year`);
    shouldThrow(RangeError, () => monthDay.with({ year: 300001 }), `${calendar} PlainMonthDay.with year range`);
}

shouldBe(Temporal.PlainMonthDay.from({ calendar: "gregory", era: "ce", eraYear: 2021, month: 4, monthCode: "M04", day: 1 }).monthCode, "M04", "PlainMonthDay era and eraYear identify the year");

for (const { calendar, year, month, monthCode, daysInMonth } of [
    { calendar: "chinese", year: 2012, month: 5, monthCode: "M04L", daysInMonth: 29 },
    { calendar: "dangi", year: 2020, month: 5, monthCode: "M04L", daysInMonth: 29 },
]) {
    const date = Temporal.PlainDate.from({ calendar, year, month, monthCode, day: 1 });
    shouldBe(date.daysInMonth, daysInMonth, `${calendar} direct daysInMonth`);
    shouldBe(date.monthsInYear, 13, `${calendar} direct monthsInYear`);
    shouldBe(Temporal.PlainDate.from({ calendar, year, month: 1e9, day: 1 }).month, 13, `${calendar} large ordinal constrains`);
    shouldThrow(RangeError, () => Temporal.PlainDate.from({ calendar, year, month: 1e9, day: 1 }, { overflow: "reject" }), `${calendar} large ordinal rejects`);
    shouldThrow(RangeError, () => Temporal.PlainDate.from({ calendar, year: 300001, month: 1, day: 1 }), `${calendar} large year rejects`);
}

for (const [calendar, before, leapStart, leapEnd, after] of [
    ["chinese", "2012-05-20", "2012-05-21", "2012-06-18", "2012-06-19"],
    ["dangi", "2020-05-22", "2020-05-23", "2020-06-20", "2020-06-21"],
]) {
    const dates = [before, leapStart, leapEnd, after].map(date => Temporal.PlainDate.from(`${date}[u-ca=${calendar}]`));
    shouldBe(`${dates[0].month}/${dates[0].monthCode}/${dates[0].day}`, "4/M04/30", `${calendar} before leap boundary`);
    shouldBe(`${dates[1].month}/${dates[1].monthCode}/${dates[1].day}`, "5/M04L/1", `${calendar} leap boundary start`);
    shouldBe(`${dates[2].month}/${dates[2].monthCode}/${dates[2].day}`, "5/M04L/29", `${calendar} leap boundary end`);
    shouldBe(`${dates[3].month}/${dates[3].monthCode}/${dates[3].day}`, "6/M05/1", `${calendar} after leap boundary`);
    const regularStart = Temporal.PlainDate.from(`${calendar === "chinese" ? "2012-04-21" : "2020-04-23"}[u-ca=${calendar}]`);
    shouldBe(regularStart.add({ months: 1 }).toString(), `${leapStart}[u-ca=${calendar}]`, `${calendar} baseline add parity`);
    shouldBe(dates[3].subtract({ months: 1 }).toString(), `${leapStart}[u-ca=${calendar}]`, `${calendar} baseline subtract parity`);
    shouldBe(regularStart.until(dates[3], { largestUnit: "months" }).toString(), "P2M", `${calendar} baseline until parity`);
    shouldBe(dates[3].since(regularStart, { largestUnit: "months" }).toString(), "P2M", `${calendar} baseline since parity`);
}

const hebrew = Temporal.PlainDate.from({ calendar: "hebrew", year: 5784, monthCode: "M05L", day: 1 });
shouldBe(`${hebrew.month}/${hebrew.monthCode}/${hebrew.daysInMonth}/${hebrew.monthsInYear}`, "6/M05L/30/13", "Hebrew controls unchanged");
const gregory = Temporal.PlainDate.from({ calendar: "gregory", year: 2021, month: 2, day: 1 });
shouldBe(`${gregory.daysInMonth}/${gregory.monthsInYear}`, "28/12", "non-target calendar controls unchanged");
