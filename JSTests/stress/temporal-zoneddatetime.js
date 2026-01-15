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

shouldBe(Temporal.ZonedDateTime instanceof Function, true);
shouldBe(Temporal.ZonedDateTime.length, 2);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.ZonedDateTime, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.ZonedDateTime, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.ZonedDateTime, 'prototype').configurable, false);
shouldBe(Temporal.ZonedDateTime.prototype.constructor, Temporal.ZonedDateTime);

{
    const zdt = new Temporal.ZonedDateTime(192_258_181_000_000_000n, "UTC");
    shouldBe(zdt.year, 1976);
    shouldBe(zdt.month, 2);
    shouldBe(zdt.day, 4);
    shouldBe(zdt.hour, 5);
    shouldBe(zdt.minute, 3);
    shouldBe(zdt.second, 1);
    shouldBe(zdt.millisecond, 0);
    shouldBe(zdt.microsecond, 0);
    shouldBe(zdt.nanosecond, 0);
}

{
    const zdt = new Temporal.ZonedDateTime(-13849764_999_999_999n, "UTC");
    shouldBe(zdt.year, 1969);
    shouldBe(zdt.month, 7);
    shouldBe(zdt.day, 24);
    shouldBe(zdt.hour, 16);
    shouldBe(zdt.minute, 50);
    shouldBe(zdt.second, 35);
    shouldBe(zdt.millisecond, 0);
    shouldBe(zdt.microsecond, 0);
    shouldBe(zdt.nanosecond, 1);
}

{
    const zdt = new Temporal.ZonedDateTime(-3217846_616_964_000_000_000n, "UTC");
    shouldBe(zdt.year, -100000);
    shouldBe(zdt.month, 7);
    shouldBe(zdt.day, 1);
    shouldBe(zdt.hour, 21);
    shouldBe(zdt.minute, 30);
    shouldBe(zdt.second, 36);
    shouldBe(zdt.millisecond, 0);
    shouldBe(zdt.microsecond, 0);
    shouldBe(zdt.nanosecond, 0);
}

{
    const zdt = new Temporal.ZonedDateTime(-1n, "UTC");
    shouldBe(zdt.epochMilliseconds, -1);
}

{
    shouldThrow(() => new Temporal.ZonedDateTime(0n, "[UTC]"), RangeError);
    shouldThrow(() => new Temporal.ZonedDateTime({ valueOf() { throw RangeError(1) }}), RangeError);
}
