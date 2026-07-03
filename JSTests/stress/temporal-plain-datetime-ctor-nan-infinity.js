//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg}: expected ${expected} but got ${actual}`);
}
function shouldThrow(fn, msg) {
    let threw = false;
    try { fn(); } catch (e) { threw = e instanceof RangeError; }
    if (!threw) throw new Error(`${msg}: expected RangeError`);
}

// Spec-positive: time-arg undefined defaults to 0 (Steps 5-10).
shouldBe(new Temporal.PlainDateTime(2024, 1, 1).toString(),
    "2024-01-01T00:00:00", "time args omitted");
shouldBe(new Temporal.PlainDateTime(2024, 1, 1, undefined).toString(),
    "2024-01-01T00:00:00", "explicit undefined hour");
shouldBe(new Temporal.PlainDateTime(2024, 1, 1, 12, undefined).toString(),
    "2024-01-01T12:00:00", "explicit undefined minute");
shouldBe(new Temporal.PlainDateTime(2024, 1, 1, 12, 34, undefined, undefined, undefined, undefined).toString(),
    "2024-01-01T12:34:00", "all trailing time args undefined");

// Spec-negative: any non-finite input (NaN or ±Infinity) on date OR time args throws.
shouldThrow(() => new Temporal.PlainDateTime(NaN, 1, 1), "NaN year");
shouldThrow(() => new Temporal.PlainDateTime(undefined, 1, 1), "undefined year");
shouldThrow(() => new Temporal.PlainDateTime(), "no args (undefined year)");
shouldThrow(() => new Temporal.PlainDateTime(Infinity, 1, 1), "Infinity year");
shouldThrow(() => new Temporal.PlainDateTime(2024, NaN, 1), "NaN month");
shouldThrow(() => new Temporal.PlainDateTime(2024, 1, NaN), "NaN day");
shouldThrow(() => new Temporal.PlainDateTime(2024, 1, 1, NaN), "NaN hour (not undefined)");
shouldThrow(() => new Temporal.PlainDateTime(2024, 1, 1, Infinity), "Infinity hour");
shouldThrow(() => new Temporal.PlainDateTime(2024, 1, 1, 12, NaN), "NaN minute (not undefined)");
shouldThrow(() => new Temporal.PlainDateTime(2024, 1, 1, 12, 34, 56, NaN), "NaN millisecond");
shouldThrow(() => new Temporal.PlainDateTime(2024, 1, 1, 12, 34, 56, 789, NaN), "NaN microsecond");
shouldThrow(() => new Temporal.PlainDateTime(2024, 1, 1, 12, 34, 56, 789, 123, NaN), "NaN nanosecond");
