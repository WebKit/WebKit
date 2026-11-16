//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}
function shouldThrow(ctor, fn) {
    try { fn(); } catch (e) {
        if (!(e instanceof ctor))
            throw new Error(`expected ${ctor.name}, got ${e}`);
        return e;
    }
    throw new Error(`expected ${ctor.name} but no exception thrown`);
}

// A bad timeZone must be reported from timeZone's own slot, before year is even read.
{
    const log = [];
    shouldThrow(TypeError, () => Temporal.Duration.from({ days: 1 }).total({ unit: 'days', relativeTo: {
        get day() { log.push('day'); return 1; },
        get timeZone() { log.push('timeZone'); return 5; },
        get month() { log.push('month'); return 1; },
        get year() { log.push('year'); throw new EvalError('year must not be reached'); },
    } }));
    shouldBe(log.join(','), 'day,month,timeZone');
}

// The same holds when a later field would raise a missing-field TypeError: the timeZone conversion
// runs first because timeZone is read before year.
{
    shouldThrow(TypeError, () => Temporal.Duration.from({ days: 1 })
        .total({ unit: 'days', relativeTo: { timeZone: 123, month: 1, monthCode: 'M01' } }));
    shouldThrow(TypeError, () => Temporal.Duration.from({ days: 1 })
        .round({ largestUnit: 'year', relativeTo: { timeZone: {}, month: 1, day: 1 } }));
}

// Every field is read, in alphabetical order, before the missing-field TypeError.
{
    const log = [];
    const names = ['calendar', 'day', 'hour', 'microsecond', 'millisecond', 'minute', 'month', 'monthCode', 'nanosecond', 'offset', 'second', 'timeZone', 'year'];
    const bag = {};
    for (const name of names)
        Object.defineProperty(bag, name, { get() { log.push(name); return undefined; }, enumerable: true });
    shouldThrow(TypeError, () => Temporal.Duration.from({ days: 1 }).total({ unit: 'days', relativeTo: bag }));
    shouldBe(log.join(','), names.join(','));
}

// A well-formed zoned relativeTo still resolves, and the timeZone getter runs exactly once.
{
    let calls = 0;
    const bag = { year: 2024, month: 1, day: 1, get timeZone() { calls++; return 'UTC'; } };
    shouldBe(Temporal.Duration.from({ hours: 48 }).round({ largestUnit: 'day', relativeTo: bag }).toString(), 'P2D');
    shouldBe(calls, 1);

    shouldBe(Temporal.Duration.from({ hours: 24 }).total({ unit: 'day', relativeTo: { year: 2024, month: 1, day: 1, timeZone: 'UTC' } }), 1);
    shouldBe(Temporal.Duration.from({ hours: 24 }).total({ unit: 'day', relativeTo: { year: 2024, month: 1, day: 1 } }), 1);
}

// An invalid time zone string is a RangeError from the same slot, not a TypeError.
{
    const log = [];
    shouldThrow(RangeError, () => Temporal.Duration.from({ days: 1 }).total({ unit: 'days', relativeTo: {
        get day() { log.push('day'); return 1; },
        get month() { log.push('month'); return 1; },
        get timeZone() { log.push('timeZone'); return 'Not/AZone'; },
        get year() { log.push('year'); return 2024; },
    } }));
    shouldBe(log.join(','), 'day,month,timeZone');
}
