//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}
// Bug: getPossibleEpochNanosecondsFor returned 0 candidates (false gap) for pre-LMT dates
// in named timezones. ICU4C's ucal_setDateTime snaps year 1385 → 1884 (first recorded year),
// making the consistency check fail (1884-offset ≠ 1385-midnight) and returning empty.
//
// Fix: detect when mismatch > 1 day (not a real DST gap of ±hours), retry using the
// forward direction (getOffsetMsAtEpoch) which ICU4C handles correctly for pre-LMT epochs.
// This matches icu4x zoneinfo64/src/lib.rs:394-402: use type_offsets[0] for pre-first-transition.
// PlainDate.toZonedDateTime (uses getStartOfDay → getPossibleEpochNanosecondsFor)
{
    const ep = -(2n**64n); // year 1385 in America/Vancouver
    const zdt = new Temporal.ZonedDateTime(ep, "America/Vancouver");
    shouldBe(zdt.year, 1385);
    // This was returning 1884-01-01 instead of 1385-06-11T00:00:00
    const startOfDay = zdt.toPlainDate().toZonedDateTime({timeZone: "America/Vancouver"});
    shouldBe(startOfDay.year, 1385);
    shouldBe(startOfDay.month, 6);
    shouldBe(startOfDay.day, 11);
    shouldBe(startOfDay.hour, 0);
    shouldBe(startOfDay.minute, 0);
    shouldBe(startOfDay.second, 0);
}
// ZonedDateTime rounding to 1 day (uses getStartOfDay internally)
{
    const ep = -(2n**64n);
    const zdt = new Temporal.ZonedDateTime(ep, "America/Vancouver");
    const rounded = zdt.round({smallestUnit: "days", roundingMode: "ceil"});
    shouldBe(rounded.year, 1385);
    shouldBe(rounded.month, 6);
    shouldBe(rounded.day, 12);
}
// Europe/Amsterdam pre-LMT dates also affected
{
    const ep = -(2n**64n);
    const zdt = new Temporal.ZonedDateTime(ep, "Europe/Amsterdam");
    const startOfDay = zdt.toPlainDate().toZonedDateTime({timeZone: "Europe/Amsterdam"});
    shouldBe(startOfDay.year, zdt.year);
    shouldBe(startOfDay.hour, 0);
    shouldBe(startOfDay.minute, 0);
}
// Real DST gaps still work correctly (not falsely detected as pre-LMT)
{
    // 2025-03-09 02:30 America/Vancouver is in a DST gap
    const earlier = Temporal.ZonedDateTime.from(
        {year:2025, month:3, day:9, hour:2, minute:30, second:0, timeZone:"America/Vancouver"},
        {disambiguation:"earlier"});
    const later = Temporal.ZonedDateTime.from(
        {year:2025, month:3, day:9, hour:2, minute:30, second:0, timeZone:"America/Vancouver"},
        {disambiguation:"later"});
    // earlier and later should give different results for a real gap
    const isAmbiguous = !earlier.equals(later);
    shouldBe(isAmbiguous, true); // 2:30 AM on DST spring-forward is non-existent → different results
}
// Additional timezones affected by pre-LMT fix (diversity check)
// Asia/Tokyo and Pacific/Auckland also have LMT→standard transitions in the pre-1900 era.
{
    const ep = -(2n**64n);
    // Asia/Tokyo: same epoch as the Vancouver test
    const zdtTokyo = new Temporal.ZonedDateTime(ep, 'Asia/Tokyo');
    shouldBe(zdtTokyo.year, 1385);
    const startTokyo = zdtTokyo.toPlainDate().toZonedDateTime({ timeZone: 'Asia/Tokyo' });
    shouldBe(startTokyo.year, 1385);
    shouldBe(startTokyo.hour, 0);
    shouldBe(startTokyo.minute, 0);
    // Pacific/Auckland: same fix path
    const zdtAuckland = new Temporal.ZonedDateTime(ep, 'Pacific/Auckland');
    shouldBe(zdtAuckland.year, 1385);
    const startAuckland = zdtAuckland.toPlainDate().toZonedDateTime({ timeZone: 'Pacific/Auckland' });
    shouldBe(startAuckland.year, 1385);
    shouldBe(startAuckland.hour, 0);
}
// ZonedDateTime.from(zdt.toString()) roundtrip must preserve epoch for pre-LMT dates (zonedroundtrip)
// The false-gap fix ensures that from() can resolve the local time back to the original epoch.
{
    const ep = -(2n**64n);
    for (const tz of ['America/Vancouver', 'Europe/Amsterdam', 'Asia/Tokyo']) {
        const zdt = new Temporal.ZonedDateTime(ep, tz);
        const rt = Temporal.ZonedDateTime.from(zdt.toString());
        shouldBe(rt.epochNanoseconds, ep);
    }
}
