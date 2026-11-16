//@ requireOptions("--useTemporal=1")

function shouldBe(a, b, msg) {
    if (a !== b) throw new Error(`${msg}: expected ${b}, got ${a}`);
}

{
    const before = Temporal.ZonedDateTime.from("2024-03-09T12:00[America/New_York]");
    shouldBe(before.offset, "-05:00", "March 9 is EST");
    const after = before.add({ days: 1 });
    shouldBe(after.offset, "-04:00", "March 10 is EDT");
    shouldBe(after.hour, 12, "Hour preserved across spring-forward");
    shouldBe(after.day, 10, "Day is March 10");
}

{
    const before = Temporal.ZonedDateTime.from("2024-11-02T12:00[America/New_York]");
    shouldBe(before.offset, "-04:00", "Nov 2 is EDT");
    const after = before.add({ days: 1 });
    shouldBe(after.offset, "-05:00", "Nov 3 is EST");
    shouldBe(after.hour, 12, "Hour preserved across fall-back");
    shouldBe(after.day, 3, "Day is Nov 3");
}

{
    const springForward = Temporal.ZonedDateTime.from("2024-03-10T12:00[America/New_York]");
    shouldBe(springForward.hoursInDay, 23, "Spring-forward day is 23 hours");
}

{
    const fallBack = Temporal.ZonedDateTime.from("2024-11-03T12:00[America/New_York]");
    shouldBe(fallBack.hoursInDay, 25, "Fall-back day is 25 hours");
}

{
    const normal = Temporal.ZonedDateTime.from("2024-06-15T12:00[America/New_York]");
    shouldBe(normal.hoursInDay, 24, "Normal day is 24 hours");
}

{
    const utc = Temporal.ZonedDateTime.from("2024-03-10T12:00[UTC]");
    shouldBe(utc.hoursInDay, 24, "UTC day is always 24 hours");
}

{
    const start = Temporal.ZonedDateTime.from("2024-03-09T23:00[America/New_York]");
    const end = Temporal.ZonedDateTime.from("2024-03-10T03:00[America/New_York]");
    const dur = start.until(end, { largestUnit: "hours" });
    // 23:00 EST to 03:00 EDT = 3 hours (not 4, because of spring-forward)
    shouldBe(dur.hours, 3, "Spring-forward: 23:00→03:00 is 3h not 4h");
}

{
    const start = Temporal.ZonedDateTime.from("2024-11-02T23:00[America/New_York]");
    const end = Temporal.ZonedDateTime.from("2024-11-03T03:00[America/New_York]");
    const dur = start.until(end, { largestUnit: "hours" });
    // 23:00 EDT to 03:00 EST = 5 hours (not 4, because of fall-back)
    shouldBe(dur.hours, 5, "Fall-back: 23:00→03:00 is 5h not 4h");
}

{
    const start = Temporal.ZonedDateTime.from("2024-03-09T23:00[America/New_York]");
    const end = Temporal.ZonedDateTime.from("2024-03-10T03:00[America/New_York]");
    const dur = end.since(start, { largestUnit: "hours" });
    shouldBe(dur.hours, 3, "since should match until for spring-forward");
}

{
    const zdt = Temporal.ZonedDateTime.from("2024-03-10T18:00[America/New_York]");
    const rounded = zdt.round({ smallestUnit: "day" });
    // 18:00 on a 23-hour day → rounds to next day (18 > 23/2 = 11.5)
    shouldBe(rounded.day, 11, "Rounding 18:00 on 23h day → next day");
}

{
    const zdt = Temporal.ZonedDateTime.from("2024-03-10T12:00[America/New_York]");
    const result = zdt.subtract({ days: 1 });
    shouldBe(result.day, 9, "Subtract 1 day from March 10");
    shouldBe(result.hour, 12, "Hour preserved");
    shouldBe(result.offset, "-05:00", "March 9 is EST");
}
