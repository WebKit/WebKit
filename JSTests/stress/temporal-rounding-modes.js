//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

const modes = ["ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor", "halfExpand", "halfTrunc", "halfEven"];

// --- PlainTime.round with each mode ---
{
    const time = Temporal.PlainTime.from("12:34:56");
    const expected = {
        ceil: 13, floor: 12, expand: 13, trunc: 12,
        halfCeil: 13, halfFloor: 13, halfExpand: 13, halfTrunc: 13, halfEven: 13,
    };
    for (const mode of modes) {
        const result = time.round({ smallestUnit: "hour", roundingMode: mode });
        shouldBe(result.hour, expected[mode], `12:34:56 round hour ${mode}`);
    }
}

{
    const time = Temporal.PlainTime.from("12:30:00");
    const expected = {
        ceil: 13, floor: 12, expand: 13, trunc: 12,
        halfCeil: 13, halfFloor: 12, halfExpand: 13, halfTrunc: 12, halfEven: 12,
    };
    for (const mode of modes) {
        const result = time.round({ smallestUnit: "hour", roundingMode: mode });
        shouldBe(result.hour, expected[mode], `12:30 exact half round hour ${mode}`);
    }
}

{
    const time = Temporal.PlainTime.from("12:29:59");
    const expected = {
        ceil: 13, floor: 12, expand: 13, trunc: 12,
        halfCeil: 12, halfFloor: 12, halfExpand: 12, halfTrunc: 12, halfEven: 12,
    };
    for (const mode of modes) {
        const result = time.round({ smallestUnit: "hour", roundingMode: mode });
        shouldBe(result.hour, expected[mode], `12:29:59 below half round hour ${mode}`);
    }
}

{
    const dur = Temporal.Duration.from({ hours: 5, minutes: 30 });
    const expected = {
        ceil: 6, floor: 5, expand: 6, trunc: 5,
        halfCeil: 6, halfFloor: 5, halfExpand: 6, halfTrunc: 5, halfEven: 6,
    };
    for (const mode of modes) {
        const result = dur.round({
            largestUnit: "hours",
            smallestUnit: "hours",
            roundingMode: mode,
        });
        shouldBe(result.hours, expected[mode], `5h30m round hour ${mode}`);
    }
}

// Negative duration — sign-aware rounding
{
    const dur = Temporal.Duration.from({ hours: -5, minutes: -30 });
    const expected = {
        ceil: -5, floor: -6, expand: -6, trunc: -5,
        halfCeil: -5, halfFloor: -6, halfExpand: -6, halfTrunc: -5, halfEven: -6,
    };
    for (const mode of modes) {
        const result = dur.round({
            largestUnit: "hours",
            smallestUnit: "hours",
            roundingMode: mode,
        });
        shouldBe(result.hours, expected[mode], `-5h30m round hour ${mode}`);
    }
}

// --- Instant.round with each mode ---
{
    // 2024-01-01T12:30:00Z → round to hour
    const inst = Temporal.Instant.from("2024-01-01T12:30:00Z");
    for (const mode of modes) {
        const result = inst.round({ smallestUnit: "hour", roundingMode: mode });
        // Result must be on a whole-hour boundary (epochNs divisible by 3_600_000_000_000).
        shouldBe(result.epochNanoseconds % 3600000000000n === 0n, true, `Instant round hour ${mode} on boundary`);
    }
}

{
    const pdt = Temporal.PlainDateTime.from("2024-01-15T12:30:00");
    for (const mode of modes) {
        const result = pdt.round({ smallestUnit: "hour", roundingMode: mode });
        shouldBe(result.hour === 12 || result.hour === 13, true, `PDT round hour ${mode} valid`);
    }
}

{
    const dur = Temporal.Duration.from({ seconds: 1, milliseconds: 500 });
    shouldBe(dur.toString({ fractionalSecondDigits: 0 }), "PT1S", "fsd 0 truncates sub-second");
    shouldBe(dur.toString({ fractionalSecondDigits: 1 }), "PT1.5S", "fsd 1");
    shouldBe(dur.toString({ fractionalSecondDigits: 3 }), "PT1.500S", "fsd 3");
    shouldBe(dur.toString({ fractionalSecondDigits: 9 }), "PT1.500000000S", "fsd 9");
}
