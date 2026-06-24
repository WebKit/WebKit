//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

function approxEqual(a, b, tolerance, msg) {
    if (Math.abs(a - b) > tolerance) throw new Error(`${msg}: expected ~${b}, got ${a}`);
}

{
    const d = Temporal.Duration.from({ months: 1, days: 15 });
    const result = d.round({
        largestUnit: "months",
        smallestUnit: "months",
        roundingMode: "halfExpand",
        relativeTo: Temporal.PlainDate.from("2024-01-01"),
    });
    // Jan 2024 → Feb 2024 has 29 days. 1 month + 15/29 ≈ 1.52 months → rounds to 2 (halfExpand rounds 0.48 to 1, so 1+1=2)
    shouldBe(result.months, 2, "round months halfExpand");
}

{
    const d = Temporal.Duration.from({ months: 1, days: 20 });
    const result = d.round({
        largestUnit: "months",
        smallestUnit: "months",
        roundingMode: "halfExpand",
        relativeTo: Temporal.PlainDate.from("2024-01-01"),
    });
    // 1 month + 20 days out of 29 remaining in Feb = 1.69 months → rounds to 2
    shouldBe(result.months, 2, "round months halfExpand up");
}

{
    const d = Temporal.Duration.from({ months: 1 });
    const total = d.total({
        unit: "days",
        relativeTo: Temporal.PlainDate.from("2024-01-01"),
    });
    // January has 31 days
    shouldBe(total, 31, "1 month from Jan 1 = 31 days");
}

{
    const d = Temporal.Duration.from({ months: 1 });
    const total = d.total({
        unit: "days",
        relativeTo: Temporal.PlainDate.from("2024-02-01"),
    });
    // February 2024 has 29 days (leap year)
    shouldBe(total, 29, "1 month from Feb 1 2024 = 29 days");
}

{
    const d = Temporal.Duration.from({ hours: 24 });
    const plainResult = d.round({ largestUnit: "days", relativeTo: Temporal.PlainDate.from("2024-03-10") });
    shouldBe(plainResult.days, 1, "24h = 1 day with PlainDate relativeTo");
    const zdtResult = d.round({ largestUnit: "days",
        relativeTo: Temporal.ZonedDateTime.from("2024-03-10T00:00[America/New_York]") });
    shouldBe(zdtResult.days, 1, "24h with ZDT on spring-forward = 1 day + 1h");
    shouldBe(zdtResult.hours, 1, "1 extra hour because day is only 23h");
}

{
    const d = Temporal.Duration.from({ days: 1 });
    const plainTotal = d.total({ unit: "hours", relativeTo: Temporal.PlainDate.from("2024-03-10") });
    shouldBe(plainTotal, 24, "1 day = 24h with PlainDate");
    const zdtTotal = d.total({ unit: "hours",
        relativeTo: Temporal.ZonedDateTime.from("2024-03-10T00:00[America/New_York]") });
    shouldBe(zdtTotal, 23, "1 day = 23h with ZDT on spring-forward");
}

{
    const d1 = Temporal.Duration.from({ days: 1 });
    const d2 = Temporal.Duration.from({ hours: 24 });
    const plainCmp = Temporal.Duration.compare(d1, d2, { relativeTo: Temporal.PlainDate.from("2024-03-10") });
    shouldBe(plainCmp, 0, "1 day == 24h with PlainDate");
    const zdtCmp = Temporal.Duration.compare(d1, d2, {
        relativeTo: Temporal.ZonedDateTime.from("2024-03-10T00:00[America/New_York]") });
    shouldBe(zdtCmp, -1, "1 day (23h) < 24h with ZDT on spring-forward");
}
