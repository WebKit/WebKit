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

// ParseTemporalTimeZoneString Step 3 allows the annotation on any of 6 productions, not just the
// two datetime ones. https://tc39.es/proposal-temporal/#sec-temporal-parsetemporaltimezonestring
{
    let cases = [
        ["2024-12[Europe/Berlin]", "Europe/Berlin"],   // TemporalYearMonthString + bracket
        ["--12-25[Europe/Berlin]", "Europe/Berlin"],   // TemporalMonthDayString + bracket
        ["12:00[Europe/Berlin]", "Europe/Berlin"],     // TemporalTimeString + bracket
    ];
    for (let [tzLike, expected] of cases) {
        shouldBe(Temporal.Now.zonedDateTimeISO(tzLike).timeZoneId, expected);
        shouldBe(Temporal.ZonedDateTime.from("2024-06-15T12:00[UTC]").withTimeZone(tzLike).timeZoneId, expected);
        shouldBe(Temporal.Instant.fromEpochNanoseconds(0n).toZonedDateTimeISO(tzLike).timeZoneId, expected);
    }

    // The annotation is what makes those acceptable, not the production.
    shouldThrow(() => Temporal.Now.zonedDateTimeISO("2024-12"), RangeError);
    shouldThrow(() => Temporal.Now.zonedDateTimeISO("--12-25"), RangeError);
    shouldThrow(() => Temporal.Now.zonedDateTimeISO("12:00"), RangeError);
}

// Step 7 delegates to ParseTimeZoneIdentifier, which only accepts UTCOffset[~SubMinutePrecision].
{
    shouldThrow(() => Temporal.Now.zonedDateTimeISO("+01:00:30"), RangeError);
    shouldThrow(() => Temporal.Now.zonedDateTimeISO("2024-01-01T00:00[+01:00:30]"), RangeError);
}

// The 4th caller of ToTemporalTimeZoneIdentifier; the other three are covered above.
{
    let instant = Temporal.Instant.fromEpochNanoseconds(0n);
    for (let tzLike of ["2024-12[Europe/Berlin]", "--12-25[Europe/Berlin]", "12:00[Europe/Berlin]"])
        shouldBe(instant.toString({ timeZone: tzLike }), "1970-01-01T01:00:00+01:00");

    // toLocaleString goes through Intl.DateTimeFormat instead, which rejects annotated strings.
    shouldThrow(() => instant.toLocaleString("en", { timeZone: "12:00[Europe/Berlin]" }), RangeError);
}

// An offset with no annotation matches TemporalDateTimeString[~Zoned]; [+Zoned] requires one.
{
    shouldBe(Temporal.Now.zonedDateTimeISO("2024-01-01T00:00+01:00").timeZoneId, "+01:00");
    shouldBe(Temporal.Now.zonedDateTimeISO("2024-01-01T00:00Z").timeZoneId, "UTC");
}

// Every Temporal.Now entry point resolves its timeZone argument through the same AO.
{
    shouldBe(Temporal.Now.plainDateISO("2024-12[Europe/Berlin]") instanceof Temporal.PlainDate, true);
    shouldBe(Temporal.Now.plainDateTimeISO("12:00[Europe/Berlin]") instanceof Temporal.PlainDateTime, true);
    shouldBe(Temporal.Now.plainTimeISO("--12-25[Europe/Berlin]") instanceof Temporal.PlainTime, true);
}

// Step 2 commits to the ParseTimeZoneIdentifier reading. "T12+01" is both a TimeZoneIANAName and a
// TemporalTimeString with an offset, so falling through to Step 3 would give "+01:00" rather than rejecting an unavailable named zone.
{
    let instant = Temporal.Instant.fromEpochNanoseconds(0n);
    shouldThrow(() => instant.toString({ timeZone: "T12+01" }), RangeError);
    shouldThrow(() => Temporal.Now.zonedDateTimeISO("T12+01"), RangeError);
    shouldThrow(() => new Temporal.ZonedDateTime(0n, "T12+01"), RangeError);

    // "Z" is an unavailable TimeZoneIANAName; only Step 6 gives it its UTC meaning.
    shouldThrow(() => new Temporal.ZonedDateTime(0n, "Z"), RangeError);
    shouldThrow(() => instant.toString({ timeZone: "Z" }), RangeError);
    shouldBe(Temporal.Now.zonedDateTimeISO("2024-01-01T00:00Z").timeZoneId, "UTC");
}

// ParseTimeZoneIdentifier Steps 5-8: offset identifiers are minute-granular and sign-normalized.
{
    let cases = [
        ["+01:00", "+01:00"],
        ["+0530", "+05:30"],   // UTCOffset accepts the colon-less form
        ["-05:00", "-05:00"],
        ["-00:00", "+00:00"],  // negative zero formats as "+00:00"
        ["+23:59", "+23:59"],
        ["-12:00", "-12:00"],
    ];
    for (let [tz, expected] of cases) {
        let zdt = new Temporal.ZonedDateTime(0n, tz);
        shouldBe(zdt.timeZoneId, expected);
        shouldBe(zdt.offset, expected);
    }
    shouldThrow(() => new Temporal.ZonedDateTime(0n, "+24:00"), RangeError);

    // The constructor takes ParseTimeZoneIdentifier, so datetime strings are not identifiers here.
    shouldThrow(() => new Temporal.ZonedDateTime(0n, "2024-01-01T00:00+01:00"), RangeError);
    shouldThrow(() => new Temporal.ZonedDateTime(0n, "2024-01-01T00:00[Europe/Berlin]"), RangeError);
    shouldBe(Temporal.Now.zonedDateTimeISO("2024-01-01T00:00[Europe/Berlin]").timeZoneId, "Europe/Berlin");
}

// Step 3 accepts a syntactically valid name without checking availability; the constructor's
// Step 6.b rejects it. "." and "./." are grammatical, "Hey/" and "_/-" are not.
{
    for (let tz of ["Foo/Bar", ".", "./.", "Hey/", "/Hey", "_/-", ""])
        shouldThrow(() => new Temporal.ZonedDateTime(0n, tz), RangeError);

    // Every TimeZoneIANANameComponent must be non-empty and start with a TZLeadingChar, not just the first.
    for (let tz of ["a/", "a/-", "a/-b", "a//b"])
        shouldThrow(() => Temporal.PlainDate.from(`2007-01-09T03:24:30+01:00[${tz}]`), RangeError);

    // Case-normalization happens in Step 6.c, via GetAvailableNamedTimeZoneIdentifier.
    shouldBe(new Temporal.ZonedDateTime(0n, "europe/berlin").timeZoneId, "Europe/Berlin");
    shouldBe(new Temporal.ZonedDateTime(0n, "UTC").timeZoneId, "UTC");
}
