//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}
// Bug: PlainDateTime.toZonedDateTime("+00:00") wrongly returned timeZoneId "UTC"
// because intlAvailableNamedTimeZone("+00:00") resolved the offset to the UTC IANA entry.
// Offset timezone identifiers must be preserved as-is, not canonicalized to a named zone.
// PlainDateTime.toZonedDateTime preserves offset timezone identifier
shouldBe(new Temporal.PlainDateTime(2020, 1, 1).toZonedDateTime("+00:00").timeZoneId, "+00:00");
shouldBe(new Temporal.PlainDateTime(2020, 1, 1).toZonedDateTime("+05:30").timeZoneId, "+05:30");
shouldBe(new Temporal.PlainDateTime(2020, 1, 1).toZonedDateTime("-08:00").timeZoneId, "-08:00");
// PlainDate.toZonedDateTime preserves offset timezone identifier
shouldBe(new Temporal.PlainDate(2020, 1, 1).toZonedDateTime("+00:00").timeZoneId, "+00:00");
shouldBe(new Temporal.PlainDate(2020, 1, 1).toZonedDateTime({ timeZone: "+00:00" }).timeZoneId, "+00:00");
// Parsing preserves offset timezone identifier (was already correct)
shouldBe(Temporal.ZonedDateTime.from("2020-01-01T00:00:00+00:00[+00:00]").timeZoneId, "+00:00");
// Round-trip: toZonedDateTime then compare to parsed string
const zdt1 = new Temporal.PlainDateTime(2020, 1, 1).toZonedDateTime("+00:00");
const zdt2 = Temporal.ZonedDateTime.from("2020-01-01T00:00:00+00:00[+00:00]");
shouldBe(zdt1.timeZoneId, "+00:00");
shouldBe(zdt2.timeZoneId, "+00:00");
shouldBe(zdt1.equals(zdt2), true);
// UTC and +00:00 are different timezone identifiers
shouldBe(new Temporal.PlainDateTime(2020, 1, 1).toZonedDateTime("UTC").timeZoneId, "UTC");
shouldBe(new Temporal.PlainDateTime(2020, 1, 1).toZonedDateTime("+00:00").timeZoneId, "+00:00");
