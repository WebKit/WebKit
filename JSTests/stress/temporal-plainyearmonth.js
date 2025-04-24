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
