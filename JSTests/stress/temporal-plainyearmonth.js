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
}

{
    shouldBe(String(Temporal.PlainYearMonth.from('2007-01-09')), `2007-01`);
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
