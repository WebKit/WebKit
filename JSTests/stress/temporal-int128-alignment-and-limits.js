//@ requireOptions("--useTemporal=1")

// On Windows, WTF::Int128 used to crash Duration rounding and reject negative
// durations. See webkit.org/b/318085.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected "${expected}" but got "${actual}"`);
}

// Used to crash: round with relativeTo (splitTimeDuration).
shouldBe(Temporal.Duration.from({ months: 1 }).round({ largestUnit: "month", relativeTo: "2023-01-01" }).toString(), "P1M");
shouldBe(Temporal.Duration.from({ months: 17, days: 14 }).round({ largestUnit: "month", relativeTo: "2023-01-01" }).toString(), "P17M14D");
shouldBe(Temporal.Duration.from({ days: 40 }).round({ largestUnit: "month", relativeTo: "2023-01-01" }).toString(), "P1M9D");
shouldBe(Temporal.Duration.from({ hours: 36, minutes: 30 }).round({ largestUnit: "day", relativeTo: "2023-01-01" }).toString(), "P1DT12H30M");

// Used to crash: wall-clock-time rounding (roundTime).
shouldBe(Temporal.PlainDateTime.from("2023-01-01T12:34:56.789").round({ smallestUnit: "minute" }).toString(), "2023-01-01T12:35:00");
shouldBe(Temporal.PlainTime.from("12:34:56.789").round({ smallestUnit: "second" }).toString(), "12:34:57");

// Used to be RangeErrors: negative durations through Checked<Int128>.
shouldBe(Temporal.Duration.from("-P1D").toString(), "-P1D");
shouldBe(new Temporal.Duration(0, 0, 0, -1).toString(), "-P1D");
shouldBe(Temporal.Duration.from({ hours: -3, minutes: -30 }).toString(), "-PT3H30M");
shouldBe(Temporal.PlainDate.from("2024-06-15").since("2023-01-01", { largestUnit: "month" }).toString(), "P17M14D");
shouldBe(Temporal.Instant.from("2020-01-01T00:00:00Z").subtract({ hours: 1 }).toString(), "2019-12-31T23:00:00Z");
