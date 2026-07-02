//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

// Key test: difference spanning spring-forward where 24h assumption fails
// March 9 23:00 EST → March 11 01:00 EDT
// Naive: endTime(01:00) < startTime(23:00) → borrow → 1 day + 26h? No.
// Correct: use timezone-aware day boundary
{
    const start = Temporal.ZonedDateTime.from("2024-03-09T23:00[America/New_York]");
    const end = Temporal.ZonedDateTime.from("2024-03-11T01:00[America/New_York]");
    const dur = start.until(end, { largestUnit: "days" });
    // Real elapsed: March 9 23:00 EST → March 11 01:00 EDT
    // = 1 day (March 10, 23h due to DST) + 2 hours = 25 hours total
    // As days+hours: 1 day + 2 hours (where the "day" is the 23h spring-forward day)
    shouldBe(dur.days, 1, "days component");
    shouldBe(dur.hours, 2, "hours component");
}

{
    const start = Temporal.ZonedDateTime.from("2024-11-02T23:00[America/New_York]");
    const end = Temporal.ZonedDateTime.from("2024-11-04T01:00[America/New_York]");
    const dur = start.until(end, { largestUnit: "days" });
    // Nov 2 23:00 EDT → Nov 4 01:00 EST
    // Day 1 = Nov 3 (25h fall-back day), then 2 more hours
    shouldBe(dur.days, 1, "fall-back: days");
    shouldBe(dur.hours, 2, "fall-back: hours");
}

{
    const start = Temporal.ZonedDateTime.from("2024-06-15T10:00[America/New_York]");
    const end = Temporal.ZonedDateTime.from("2024-06-15T14:30[America/New_York]");
    const dur = start.until(end, { largestUnit: "hours" });
    shouldBe(dur.hours, 4, "same-day hours");
    shouldBe(dur.minutes, 30, "same-day minutes");
}

{
    const start = Temporal.ZonedDateTime.from("2024-01-15T12:00[America/New_York]");
    const end = Temporal.ZonedDateTime.from("2024-04-15T12:00[America/New_York]");
    const dur = start.until(end, { largestUnit: "months" });
    shouldBe(dur.months, 3, "3 months across DST");
    shouldBe(dur.days, 0, "exact month boundary");
    shouldBe(dur.hours, 0, "same wall-clock time");
}

{
    const start = Temporal.ZonedDateTime.from("2024-03-11T01:00[America/New_York]");
    const end = Temporal.ZonedDateTime.from("2024-03-09T23:00[America/New_York]");
    const dur = start.until(end, { largestUnit: "days" });
    shouldBe(dur.days, -1, "negative days");
    shouldBe(dur.hours, -2, "negative hours");
}

{
    const zdt = Temporal.ZonedDateTime.from("2024-03-10T03:00[America/New_York]");
    const dur = zdt.until(zdt, { largestUnit: "days" });
    shouldBe(dur.days, 0, "same instant: days");
    shouldBe(dur.hours, 0, "same instant: hours");
    shouldBe(dur.sign, 0, "same instant: sign");
}
