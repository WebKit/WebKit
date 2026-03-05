//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
    if (message !== undefined)
        shouldBe(String(error), message);
}

shouldBe(Temporal.PlainYearMonth instanceof Function, true);
shouldBe(Temporal.PlainYearMonth.length, 2);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainYearMonth, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainYearMonth, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainYearMonth, 'prototype').configurable, false);
shouldBe(Temporal.PlainYearMonth.prototype.constructor, Temporal.PlainYearMonth);

const yearMonth = new Temporal.PlainYearMonth(2025, 4);

{
    shouldBe(yearMonth.year, 2025);
    shouldBe(yearMonth.monthCode, "M04");
    shouldBe(yearMonth.day, undefined);
    shouldBe(yearMonth.calendarId, "iso8601");

    shouldThrow(() => new Temporal.PlainYearMonth(275761, 1), RangeError);
    shouldThrow(() => new Temporal.PlainYearMonth(2025, 20), RangeError);
}

{
    shouldBe(yearMonth.toString(), '2025-04');
    shouldBe(yearMonth.toJSON(), yearMonth.toString());
    shouldBe(yearMonth.toLocaleString(), yearMonth.toString());

    shouldThrow(() => yearMonth.toString({ calendarName: "bogus" }), RangeError);
}

{
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09'), { overflow: 'constrain' }), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09'), undefined), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09T03:24:30')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09t03:24:30')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09 03:24:30')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09T03:24:30+20:20:59')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09T03:24:30-20:20:59')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09T03:24:30+10')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09T03:24:30+1020')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09T03:24:30+102030')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09T03:24:30+10:20:30.05')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09T03:24:30+10:20:30.123456789')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09T03:24:30+01:00[Europe/Brussels]')), `2007-01`);
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09 03:24:30+01:00[Europe/Brussels]')), `2007-01`);

    let yearMonth1 = Temporal.PlainYearMonth.from('2007-04-29T03:24:30+01:00[Europe/Brussels]');
    shouldBe(yearMonth1 === Temporal.PlainYearMonth.from(yearMonth1), false);
    shouldBe(yearMonth1.toString(), Temporal.PlainYearMonth.from(yearMonth1).toString());

    shouldBe(Temporal.PlainYearMonth.from({ year: 2025, month: 4, day: 29 }).toString(), yearMonth.toString());
    shouldBe(Temporal.PlainYearMonth.from({ year: 2025, monthCode: 'M04', day: 29 }).toString(), yearMonth.toString());

    shouldBe(Temporal.PlainYearMonth.from({ year: 2025, month: 20, day: 40 }).toString(), '2025-12');
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: 2025, month: 20, day: 40 }, null), TypeError);
    shouldThrow(() => Temporal.PlainYearMonth.from({ year: 2025, month: 20, day: 40 }, { overflow: 'reject' }), RangeError);

    shouldBe(Temporal.PlainYearMonth.from({ year: 2025, month: 4 }).toString(), yearMonth.toString());
    shouldBe(Temporal.PlainYearMonth.from({ year: 2025, monthCode: 'M04' }).toString(), yearMonth.toString());

    shouldThrow(() => { Temporal.PlainYearMonth.from({ month: 4, day: 9 }); }, TypeError);
    shouldThrow(() => { Temporal.PlainYearMonth.from({ year: 2007, day: 1 }); }, TypeError);
    shouldThrow(() => { Temporal.PlainYearMonth.from({ year: Infinity, month: 9 }); }, RangeError);
    shouldThrow(() => { Temporal.PlainYearMonth.from({ year: -271822, month: 9 }); }, RangeError);
    shouldThrow(() => { Temporal.PlainYearMonth.from({ year: 2025, monthCode: 'M00' }); }, RangeError);
    shouldThrow(() => { Temporal.PlainYearMonth.from({ year: 2025, month: 0 }); }, RangeError);
    shouldThrow(() => { Temporal.PlainYearMonth.from({ year: 2025, month: 1, monthCode: 'M02' }); }, RangeError);
}

shouldBe(Temporal.PlainYearMonth.prototype.equals.length, 1);
{
    const ones = new Temporal.PlainYearMonth(1,1);
    shouldBe(ones.equals(new Temporal.PlainYearMonth(1,1)), true);
    shouldBe(ones.equals(new Temporal.PlainYearMonth(2,1)), false);
    shouldBe(ones.equals(new Temporal.PlainYearMonth(1,2)), false);
}

shouldBe(Temporal.PlainYearMonth.prototype.valueOf.length, 0);
{
    shouldThrow(() => yearMonth.valueOf(), TypeError);
}

{
    let one = Temporal.PlainYearMonth.from('1001-01');
    let two = Temporal.PlainYearMonth.from('1003-03');
    let three = Temporal.PlainYearMonth.from('1002-01');
    let four = Temporal.PlainYearMonth.from('1000-02');
    let sorted = [one, two, three, four].sort(Temporal.PlainYearMonth.compare);
    shouldBe(sorted.join(' '), `1000-02 1001-01 1002-01 1003-03`);
}

shouldBe(Temporal.PlainYearMonth.prototype.with.length, 1);
{
    shouldBe(yearMonth.with({ year: 2025, month: 4, day: 5 }).toString(), '2025-04');
    shouldBe(yearMonth.with({ year: 2025, month: 3 }).toString(), '2025-03');
    shouldBe(yearMonth.with({ month: 3 }).toString(), '2025-03');
    shouldBe(yearMonth.with({ year: 2024 }).toString(), '2024-04');
    shouldThrow(() => yearMonth.with({ day: 5 }), TypeError);

    shouldBe(yearMonth.with({ month: 13 }).toString(), '2025-12');
    shouldThrow(() => { yearMonth.with({ month: 13 }, { overflow: 'reject' }); }, RangeError);
}

shouldBe(Temporal.PlainYearMonth.prototype.toPlainDate.length, 1);
{
    shouldBe(yearMonth.toPlainDate({ day: 29 }).toString(), "2025-04-29");
    shouldThrow(() => yearMonth.toPlainDate({ notYear: 'whatever' }), TypeError);
    const leapYear = new Temporal.PlainYearMonth(2024, 2);
    shouldBe(leapYear.toPlainDate({ day: 29 }).toString(), "2024-02-29");
    const commonYear = new Temporal.PlainYearMonth(2025, 2);
    shouldBe(commonYear.toPlainDate({ day: 29 }).toString(), "2025-02-28");
}

shouldBe(Temporal.PlainYearMonth.prototype.add.length, 1);
shouldBe(Temporal.PlainYearMonth.prototype.subtract.length, 1);
{
    shouldBe(yearMonth.add(new Temporal.Duration()).toString(), '2025-04');
    shouldBe(yearMonth.add({ years: 1, months: 1 }).toString(), '2026-05');
    shouldBe(yearMonth.add('P1W1D').toString(), '2025-04');
    shouldBe(yearMonth.add({ hours: 24 }).toString(), '2025-04');
    shouldBe(yearMonth.add({ days: 36500000 }).toString(), '+101958-11');
    shouldBe(yearMonth.add(new Temporal.Duration(-1, -1, 0, -1)).toString(), '2024-03');
    shouldBe(Temporal.PlainYearMonth.from('2025-04').add({ months: 1 }).toString(), '2025-05');
    shouldThrow(() => { yearMonth.add({ years: 300000 }); }, RangeError);

    shouldBe(yearMonth.subtract(new Temporal.Duration()).toString(), '2025-04');
    shouldBe(yearMonth.subtract({ years: 1, months: 1 }).toString(), '2024-03');
    shouldBe(yearMonth.subtract('P1W1D').toString(), '2025-04');
    shouldBe(yearMonth.subtract({ hours: 24 }).toString(), '2025-04');
    shouldBe(yearMonth.subtract({ days: 36500000 }).toString(), '-097909-09');
    shouldBe(yearMonth.subtract(new Temporal.Duration(-1, -1, 0, -1)).toString(), '2026-05');
    shouldBe(Temporal.PlainYearMonth.from('2025-04').subtract({ months: 1 }).toString(), '2025-03');
    shouldThrow(() => { yearMonth.subtract({ years: 300000 }); }, RangeError);
}

shouldBe(Temporal.PlainYearMonth.prototype.since.length, 1);
shouldBe(Temporal.PlainYearMonth.prototype.until.length, 1);
{
    shouldBe(yearMonth.since('2024-04-01').toString(), 'P1Y');
    shouldBe(yearMonth.until('2024-04-01').toString(), '-P1Y');
    shouldBe(yearMonth.since('2024-04-01', { largestUnit: 'month' }).toString(), 'P12M');
    shouldBe(yearMonth.until('2024-04-01', { largestUnit: 'month' }).toString(), '-P12M');
    shouldBe(yearMonth.since('2024-03-30').toString(), 'P1Y1M');
    shouldBe(yearMonth.until('2024-03-30').toString(), '-P1Y1M');
    shouldBe(yearMonth.since('2024-03-30', { smallestUnit: 'year' }).toString(), 'P1Y');
    shouldBe(yearMonth.until('2024-03-30', { smallestUnit: 'year' }).toString(), '-P1Y');

    shouldBe(yearMonth.since('2025-06', { largestUnit: 'month' }).toString(), '-P2M');
    shouldBe(yearMonth.until('2025-06', { largestUnit: 'month' }).toString(), 'P2M');
    shouldBe(yearMonth.since('2025-06', { smallestUnit: 'year' }).toString(), 'PT0S');
    shouldBe(yearMonth.until('2025-06', { smallestUnit: 'year' }).toString(), 'PT0S');

    shouldBe(yearMonth.since('2020-03-15', { largestUnit: 'month' }).toString(), 'P61M');
    shouldBe(yearMonth.until('2020-03-15', { largestUnit: 'month' }).toString(), '-P61M');
    shouldBe(yearMonth.since('2019-12-15', { roundingMode: 'halfExpand', roundingIncrement: 3 }).toString(), 'P5Y3M');
    shouldBe(yearMonth.until('2019-12-15', { roundingMode: 'halfExpand', roundingIncrement: 3 }).toString(), '-P5Y3M');

    const earlier = Temporal.PlainYearMonth.from("2019-01");
    const later = Temporal.PlainYearMonth.from("2021-09");
    shouldBe(later.since(earlier, { smallestUnit: "years", roundingIncrement: 4, roundingMode: "halfExpand" }).toString(), "P4Y");
    shouldBe(earlier.until(later, { smallestUnit: "years", roundingIncrement: 4, roundingMode: "halfExpand" }).toString(), "P4Y");

    shouldThrow(() => { yearMonth.until('2019-02', { smallestUnit: 'week' }); }, RangeError);
    shouldThrow(() => { yearMonth.until('2019-02', { smallestUnit: 'day' }); }, RangeError);
    shouldThrow(() => { yearMonth.until('2019-02', { largestUnit: 'week' }); }, RangeError);
    shouldThrow(() => { yearMonth.until('2019-02', { largestUnit: 'year', smallestUnit: 'day' }); }, RangeError);
}
