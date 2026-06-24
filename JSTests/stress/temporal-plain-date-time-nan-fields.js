//@ requireOptions("--useTemporal=1")

function shouldThrow(fn, type) {
    let err;
    try { fn(); } catch (e) { err = e; }
    if (!(err instanceof type))
        throw new Error(`Expected ${type.name} but got ${err}`);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

// Spec uses ToIntegerWithTruncation for year, eraYear, and time fields. ToIntegerWithTruncation
// throws RangeError on NaN/+Infinity/-Infinity. Previously, JSC used toIntegerOrInfinity which
// silently maps NaN -> 0, letting NaN inputs slip through.
shouldThrow(() => Temporal.PlainDateTime.from({ year: NaN, month: 1, day: 1 }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1, hour: NaN }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1, minute: NaN }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1, second: NaN }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1, millisecond: NaN }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1, microsecond: NaN }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1, nanosecond: NaN }), RangeError);

// +/-Infinity continues to throw (was already covered by isFinite check).
shouldThrow(() => Temporal.PlainDateTime.from({ year: Infinity, month: 1, day: 1 }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: -Infinity, month: 1, day: 1 }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1, hour: Infinity }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1, nanosecond: -Infinity }), RangeError);

// eraYear with NaN throws (japanese calendar uses era/eraYear).
shouldThrow(() => Temporal.PlainDateTime.from({ era: "reiwa", eraYear: NaN, month: 1, day: 1, calendar: "japanese" }), RangeError);

// Day and month use ToPositiveIntegerWithTruncation: NaN/Infinity AND <=0 all throw,
// regardless of overflow option. (Existing spec behavior, kept by this change.)
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: NaN }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: NaN, day: 1 }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 0 }, { overflow: "constrain" }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 0, day: 1 }, { overflow: "constrain" }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: 1, day: -5 }, { overflow: "constrain" }), RangeError);
shouldThrow(() => Temporal.PlainDateTime.from({ year: 2024, month: -1, day: 1 }, { overflow: "constrain" }), RangeError);

// Sanity: valid integer-like values still succeed.
shouldBe(Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1 }).toString(), "2024-01-01T00:00:00");
shouldBe(Temporal.PlainDateTime.from({ year: 2024, month: 1, day: 1, hour: 1.7, minute: 2.9 }).toString(), "2024-01-01T01:02:00");

// Year=0 is valid (ToIntegerWithTruncation, not ToPositive).
shouldBe(Temporal.PlainDateTime.from({ year: 0, month: 1, day: 1 }).year, 0);
// Negative year is valid.
shouldBe(Temporal.PlainDateTime.from({ year: -1, month: 1, day: 1 }).year, -1);
