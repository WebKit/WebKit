//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
}

// Stage 4: Temporal.TimeZone constructor is removed
shouldThrow(() => { new Temporal.TimeZone("UTC"); }, TypeError);
shouldThrow(() => { Temporal.TimeZone.from("UTC"); }, TypeError);

// TimeZone is accessed via timeZoneId on ZonedDateTime
{
    let zdt = Temporal.ZonedDateTime.from("2024-06-15T12:00[Asia/Tokyo]");
    shouldBe(zdt.timeZoneId, "Asia/Tokyo");
    shouldBe(zdt.offset, "+09:00");
}

// UTC, fixed offset, ZonedDateTime
{
    let zdt = Temporal.ZonedDateTime.from("2024-06-15T12:00[UTC]");
    shouldBe(zdt.timeZoneId, "UTC");
    shouldBe(zdt.offset, "+00:00");
}

// Fixed UTC offset ZonedDateTime
{
    let zdt = new Temporal.ZonedDateTime(0n, "+05:30");
    shouldBe(zdt.timeZoneId, "+05:30");
    shouldBe(zdt.offset, "+05:30");
}

// Temporal.Now.timeZoneId returns a string
{
    let tzId = Temporal.Now.timeZoneId();
    shouldBe(typeof tzId, "string");
    shouldBe(tzId.length > 0, true);
}
