//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

// One nanosecond past 255 days must round up under ceil and expand, and stay put under trunc/floor.
{
    const justOver = Temporal.Duration.from({ days: 255, nanoseconds: 1 });
    shouldBe(justOver.round({ smallestUnit: 'day', roundingMode: 'ceil' }).toString(), 'P256D');
    shouldBe(justOver.round({ smallestUnit: 'day', roundingMode: 'expand' }).toString(), 'P256D');
    shouldBe(justOver.round({ smallestUnit: 'day', roundingMode: 'trunc' }).toString(), 'P255D');
    shouldBe(justOver.round({ smallestUnit: 'day', roundingMode: 'floor' }).toString(), 'P255D');
}

// One nanosecond short of 255 days must round down under floor and trunc, and up under ceil.
{
    const justUnder = Temporal.Duration.from({ days: 254, hours: 23, minutes: 59, seconds: 59, milliseconds: 999, microseconds: 999, nanoseconds: 999 });
    shouldBe(justUnder.round({ smallestUnit: 'day', roundingMode: 'floor' }).toString(), 'P254D');
    shouldBe(justUnder.round({ smallestUnit: 'day', roundingMode: 'trunc' }).toString(), 'P254D');
    shouldBe(justUnder.round({ smallestUnit: 'day', roundingMode: 'ceil' }).toString(), 'P255D');
}

// One nanosecond below the exact midpoint must round down, not up. total() legitimately reports
// 255.5 here because the nearest double to the exact value is 255.5; the rounding decision must not
// be taken from it.
{
    const justBelowHalf = Temporal.Duration.from({ days: 255, hours: 11, minutes: 59, seconds: 59, milliseconds: 999, microseconds: 999, nanoseconds: 999 });
    shouldBe(justBelowHalf.round({ smallestUnit: 'day' }).toString(), 'P255D');
    shouldBe(justBelowHalf.round({ smallestUnit: 'day', roundingMode: 'halfExpand' }).toString(), 'P255D');
    shouldBe(justBelowHalf.total({ unit: 'day' }), 255.5);
}

// An exact midpoint still breaks the tie by mode.
{
    const exactlyHalf = Temporal.Duration.from({ days: 255, hours: 12 });
    shouldBe(exactlyHalf.round({ smallestUnit: 'day', roundingMode: 'halfExpand' }).toString(), 'P256D');
    shouldBe(exactlyHalf.round({ smallestUnit: 'day', roundingMode: 'halfTrunc' }).toString(), 'P255D');
    shouldBe(exactlyHalf.round({ smallestUnit: 'day', roundingMode: 'halfEven' }).toString(), 'P256D');
}

// Negative durations round away from zero under expand and toward it under trunc.
{
    const negative = Temporal.Duration.from({ days: -255, nanoseconds: -1 });
    shouldBe(negative.round({ smallestUnit: 'day', roundingMode: 'expand' }).toString(), '-P256D');
    shouldBe(negative.round({ smallestUnit: 'day', roundingMode: 'trunc' }).toString(), '-P255D');
    shouldBe(negative.round({ smallestUnit: 'day', roundingMode: 'floor' }).toString(), '-P256D');
    shouldBe(negative.round({ smallestUnit: 'day', roundingMode: 'ceil' }).toString(), '-P255D');
}

// A rounding increment greater than one still divides evenly into the day span.
{
    const d = Temporal.Duration.from({ days: 255, nanoseconds: 1 });
    shouldBe(d.round({ smallestUnit: 'day', roundingIncrement: 2, roundingMode: 'ceil' }).toString(), 'P256D');
    shouldBe(d.round({ smallestUnit: 'day', roundingIncrement: 2, roundingMode: 'trunc' }).toString(), 'P254D');
}
