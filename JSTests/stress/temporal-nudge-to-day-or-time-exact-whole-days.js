//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

// didExpandDays must stay false when rounding moves the time within a single day.
{
    const start = Temporal.PlainDateTime.from('2000-01-31T00:00');
    const end = Temporal.PlainDateTime.from('2000-02-29T00:01');

    shouldBe(start.until(end, { largestUnit: 'month', smallestUnit: 'hour', roundingMode: 'ceil' }).toString(), 'P29DT1H');
    shouldBe(start.until(end, { largestUnit: 'month', smallestUnit: 'hour', roundingIncrement: 2, roundingMode: 'expand' }).toString(), 'P29DT2H');
    shouldBe(start.since(end, { largestUnit: 'month', smallestUnit: 'hour', roundingMode: 'floor' }).toString(), '-P29DT1H');
    shouldBe(start.withCalendar('gregory').until(end.withCalendar('gregory'), { largestUnit: 'month', smallestUnit: 'hour', roundingMode: 'ceil' }).toString(), 'P29DT1H');

    shouldBe(Temporal.PlainDateTime.from('1999-12-31T23:30').until('2000-02-29T23:59:59.999999999', { largestUnit: 'year', smallestUnit: 'minute', roundingIncrement: 2, roundingMode: 'halfExpand' }).toString(), 'P1M29DT30M');
    shouldBe(Temporal.PlainDateTime.from('1999-01-31T00:00').until('1999-02-28T00:00:01', { largestUnit: 'month', smallestUnit: 'second', roundingIncrement: 30, roundingMode: 'expand' }).toString(), 'P28DT30S');

    shouldBe(new Temporal.Duration(0, 0, 0, 29, 0, 1).round({ relativeTo: '2000-01-31', largestUnit: 'month', smallestUnit: 'hour', roundingMode: 'ceil' }).toString(), 'P29DT1H');
    shouldBe(new Temporal.Duration(0, 0, 0, 29, 0, 1).round({ relativeTo: '2000-01-31T00:00', largestUnit: 'year', smallestUnit: 'hour', roundingMode: 'expand' }).toString(), 'P29DT1H');

    // A date largestUnit never reaches the bubble, and a smallestUnit that needs no rounding
    // leaves the time duration untouched. Both were already correct.
    shouldBe(start.until(end, { largestUnit: 'day', smallestUnit: 'hour', roundingMode: 'ceil' }).toString(), 'P29DT1H');
    shouldBe(start.until(end, { largestUnit: 'week', smallestUnit: 'hour', roundingMode: 'ceil' }).toString(), 'P4W1DT1H');
    shouldBe(start.until(end, { largestUnit: 'month', smallestUnit: 'minute', roundingMode: 'ceil' }).toString(), 'P29DT1M');
}

// The whole-day count must be exact. At microsecond granularity a double stops resolving one
// microsecond in a day once the day count reaches 2^17, so 131072 days used to throw.
{
    const origin = Temporal.PlainDateTime.from('1970-01-01T00:00:00');
    const options = { smallestUnit: 'microsecond', largestUnit: 'day', roundingMode: 'trunc' };

    shouldBe(origin.until('2328-11-11T23:59:59.999999', options).toString(), 'P131071DT23H59M59.999999S');
    shouldBe(origin.until('2328-11-12T23:59:59.999999', options).toString(), 'P131072DT23H59M59.999999S');
    shouldBe(origin.until('2328-11-13T23:59:59.999999', options).toString(), 'P131073DT23H59M59.999999S');

    shouldBe(Temporal.Duration.from({ days: 131072, hours: 23, minutes: 59, seconds: 59, milliseconds: 999, microseconds: 999 })
        .round({ smallestUnit: 'microsecond', largestUnit: 'day', relativeTo: '1970-01-01', roundingMode: 'trunc' }).toString(),
        'P131072DT23H59M59.999999S');
}
