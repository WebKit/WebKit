//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

// One nanosecond past a whole number of calendar units must still round away from zero.
{
    const origin = Temporal.PlainDateTime.from('2000-01-01T00:00:00');

    shouldBe(origin.until('2008-05-01T00:00:00.000000001', { smallestUnit: 'month', largestUnit: 'month', roundingMode: 'ceil' }).toString(), 'P101M');
    shouldBe(origin.until('2008-05-01T00:00:00.000000001', { smallestUnit: 'month', largestUnit: 'month', roundingMode: 'expand' }).toString(), 'P101M');
    shouldBe(origin.until('2100-01-01T00:00:00.000000001', { smallestUnit: 'year', largestUnit: 'year', roundingMode: 'ceil' }).toString(), 'P101Y');
    shouldBe(origin.until('2008-05-01T00:00:00.000000001', { smallestUnit: 'month', largestUnit: 'month', roundingMode: 'trunc' }).toString(), 'P100M');
    shouldBe(origin.until('2008-05-01T00:00:00.000000001', { smallestUnit: 'month', largestUnit: 'month', roundingMode: 'floor' }).toString(), 'P100M');

    shouldBe(Temporal.ZonedDateTime.from('2000-01-01T00:00:00[UTC]')
        .until('2005-06-01T00:00:00.000000001[UTC]', { smallestUnit: 'day', roundingMode: 'ceil' }).toString(), 'P1979D');

    shouldBe(Temporal.Duration.from({ months: 100, nanoseconds: 1 })
        .round({ smallestUnit: 'month', largestUnit: 'month', relativeTo: '2000-01-01T00:00:00', roundingMode: 'ceil' }).toString(), 'P101M');

    // Exactly on a unit boundary must not be nudged outward by any mode.
    shouldBe(origin.until('2008-05-01T00:00:00', { smallestUnit: 'month', largestUnit: 'month', roundingMode: 'ceil' }).toString(), 'P100M');
    shouldBe(origin.until('2100-01-01T00:00:00', { smallestUnit: 'year', largestUnit: 'year', roundingMode: 'expand' }).toString(), 'P100Y');
}

// A one-nanosecond shortfall on either side of the midpoint must break the tie the right way, which
// requires resolving below one ULP of the double total.
{
    const origin = Temporal.PlainDateTime.from('2000-01-01T00:00:00');
    shouldBe(origin.until('2008-05-16T11:59:59.999999999', { smallestUnit: 'month', largestUnit: 'month', roundingMode: 'halfExpand' }).toString(), 'P100M');
}

// halfEven's cardinality is (r1 / (r2 - r1)) modulo 2, so it depends on the rounding increment and
// not on r1 alone. Exercise an increment greater than one.
{
    const origin = Temporal.PlainDateTime.from('2000-01-01T00:00:00');
    shouldBe(origin.until('2000-04-01T00:00:00.000000001', { smallestUnit: 'month', largestUnit: 'month', roundingIncrement: 2, roundingMode: 'ceil' }).toString(), 'P4M');
    shouldBe(origin.until('2000-03-01T00:00:00', { smallestUnit: 'month', largestUnit: 'month', roundingIncrement: 2, roundingMode: 'ceil' }).toString(), 'P2M');
    shouldBe(origin.until('2000-03-01T00:00:00.000000001', { smallestUnit: 'month', largestUnit: 'month', roundingIncrement: 2, roundingMode: 'ceil' }).toString(), 'P4M');
}
