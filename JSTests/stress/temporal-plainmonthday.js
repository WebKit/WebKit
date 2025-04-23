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

shouldBe(Temporal.PlainMonthDay instanceof Function, true);
shouldBe(Temporal.PlainMonthDay.length, 2);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainMonthDay, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainMonthDay, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.PlainMonthDay, 'prototype').configurable, false);
shouldBe(Temporal.PlainMonthDay.prototype.constructor, Temporal.PlainMonthDay);

const monthDay = new Temporal.PlainMonthDay(4, 29);

{
    shouldBe(monthDay.monthCode, "M04");
    shouldBe(monthDay.day, 29);
    shouldBe(monthDay.year, undefined);
    shouldBe(monthDay.calendarId, "iso8601");

    shouldThrow(() => new Temporal.PlainMonthDay(20, 1), RangeError);
    shouldThrow(() => new Temporal.PlainMonthDay(1, 40), RangeError);
}

{
    shouldBe(monthDay.toString(), '04-29');
    shouldBe(monthDay.toJSON(), monthDay.toString());
    shouldBe(monthDay.toLocaleString(), monthDay.toString());
}

shouldBe(Temporal.PlainMonthDay.prototype.with.length, 1);
{
    shouldBe(monthDay.with({ year: 2021, month: 3, day: 5 }).toString(), '03-05');
    shouldBe(monthDay.with({ month: 3, day: 5 }).toString(), '03-05');
    shouldBe(monthDay.with({ month: 3 }).toString(), '03-29');
    shouldBe(monthDay.with({ day: 5 }).toString(), '04-05');

    shouldBe(monthDay.with({ day: 31 }).toString(), '04-30');
    shouldThrow(() => { monthDay.with({ day: 31 }, { overflow: 'reject' }); }, RangeError);
}

{
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09T03:24:30')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09t03:24:30')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09 03:24:30')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09T03:24:30+20:20:59')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09T03:24:30-20:20:59')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09T03:24:30+10')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09T03:24:30+1020')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09T03:24:30+102030')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09T03:24:30+10:20:30.05')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09T03:24:30+10:20:30.123456789')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09T03:24:30+01:00[Europe/Brussels]')), `01-09`);
    shouldBe(String(Temporal.PlainMonthDay.from('2007-01-09 03:24:30+01:00[Europe/Brussels]')), `01-09`);

    let monthDay1 = Temporal.PlainMonthDay.from('2007-04-29T03:24:30+01:00[Europe/Brussels]');
    shouldBe(monthDay1 === Temporal.PlainMonthDay.from(monthDay1), false);
    shouldBe(monthDay1.toString(), Temporal.PlainMonthDay.from(monthDay1).toString());

    shouldBe(Temporal.PlainMonthDay.from({ year: 2007, month: 4, day: 29 }).toString(), monthDay.toString());
    shouldBe(Temporal.PlainMonthDay.from({ year: 2007, monthCode: 'M04', day: 29 }).toString(), monthDay.toString());

    shouldBe(Temporal.PlainMonthDay.from({ year: 2007, month: 20, day: 40 }).toString(), '12-31');
    shouldThrow(() => Temporal.PlainMonthDay.from({ year: 2007, month: 20, day: 40 }, { overflow: 'reject' }), RangeError);

    shouldBe(Temporal.PlainMonthDay.from({ month: 4, day: 29 }).toString(), monthDay.toString());
    shouldBe(Temporal.PlainMonthDay.from({ monthCode: 'M04', day: 29 }).toString(), monthDay.toString());

    shouldThrow(() => { Temporal.PlainMonthDay.from({ year: 2007, day: 9 }); }, TypeError);
    shouldThrow(() => { Temporal.PlainMonthDay.from({ year: 2007, month: 1 }); }, TypeError);
    shouldThrow(() => { Temporal.PlainMonthDay.from({ month: Infinity, day: 9 }); }, RangeError);
    shouldThrow(() => { Temporal.PlainMonthDay.from({ month: 0, day: 9 }); }, RangeError);
    shouldThrow(() => { Temporal.PlainMonthDay.from({ monthCode: 'M00', day: 9 }); }, RangeError);
    shouldThrow(() => { Temporal.PlainMonthDay.from({ month: 1, day: 0 }); }, RangeError);
    shouldThrow(() => { Temporal.PlainMonthDay.from({ month: 1, monthCode: 'M02', day: 9 }); }, RangeError);
}

shouldBe(Temporal.PlainMonthDay.prototype.equals.length, 1);
{
    const ones = new Temporal.PlainMonthDay(1,1);
    shouldBe(ones.equals(new Temporal.PlainMonthDay(1,1)), true);
    shouldBe(ones.equals(new Temporal.PlainMonthDay(2,1)), false);
    shouldBe(ones.equals(new Temporal.PlainMonthDay(1,2)), false);
}
