function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

// Pacific/Niue moved from LMT -11:19:40 to -11:20 at local midnight on 1952-10-16,
// so 23:59:40-23:59:59 on 1952-10-15 happen twice, 20 seconds apart. round() must
// resolve the rounded wall-clock time with the exact current offset, not one
// rounded to minutes, otherwise the result slides to the earlier occurrence.

{
    const zdt = Temporal.Instant.from("1952-10-16T11:19:50Z").toZonedDateTimeISO("Pacific/Niue");
    shouldBe(zdt.offset, "-11:20");
    const rounded = zdt.round({ smallestUnit: "second" });
    shouldBe(rounded.epochNanoseconds, zdt.epochNanoseconds);
    shouldBe(rounded.offset, "-11:20");
}

{
    const zdt = Temporal.Instant.from("1952-10-16T11:19:50.400Z").toZonedDateTimeISO("Pacific/Niue");
    shouldBe(zdt.offset, "-11:20");
    const rounded = zdt.round({ smallestUnit: "second" });
    shouldBe(rounded.epochNanoseconds, Temporal.Instant.from("1952-10-16T11:19:50Z").epochNanoseconds);
    shouldBe(rounded.offset, "-11:20");
}

{
    const zdt = Temporal.Instant.from("1952-10-16T11:19:30Z").toZonedDateTimeISO("Pacific/Niue");
    shouldBe(zdt.offset, "-11:19:40");
    const rounded = zdt.round({ smallestUnit: "second" });
    shouldBe(rounded.epochNanoseconds, zdt.epochNanoseconds);
    shouldBe(rounded.offset, "-11:19:40");
}

{
    const zdt = Temporal.Instant.from("1952-10-16T11:19:59Z").toZonedDateTimeISO("Pacific/Niue");
    const rounded = zdt.round({ smallestUnit: "minute" });
    shouldBe(rounded.epochNanoseconds, Temporal.Instant.from("1952-10-16T11:20:00Z").epochNanoseconds);
    shouldBe(rounded.toString(), "1952-10-16T00:00:00-11:20[Pacific/Niue]");
}
