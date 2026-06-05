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
        throw new Error(`Expected ${errorType.name} but got ${error}`);
}

const instant1 = Temporal.Instant.fromEpochMilliseconds(0);
const instant2 = Temporal.Instant.fromEpochMilliseconds(3600 * 1000);
const time1 = Temporal.PlainTime.from("01:00");
const time2 = Temporal.PlainTime.from("02:00");
const date1 = Temporal.PlainDate.from("2000-01-01");
const date2 = Temporal.PlainDate.from("2001-03-04");
const dateTime1 = Temporal.PlainDateTime.from("2000-01-01T00:00");
const dateTime2 = Temporal.PlainDateTime.from("2001-03-04T05:06");
const yearMonth1 = Temporal.PlainYearMonth.from("2000-01");
const yearMonth2 = Temporal.PlainYearMonth.from("2001-03");
const zoned1 = new Temporal.ZonedDateTime(0n, "UTC");
const zoned2 = new Temporal.ZonedDateTime(3600_000_000_000n, "UTC");
const duration = Temporal.Duration.from({ hours: 1 });

// smallestUnit: "auto" must throw a RangeError, not crash, in every consumer.
// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalunitvaluedoption
// https://tc39.es/proposal-temporal/#sec-temporal-validatetemporalunitvalue

// round with options object
shouldThrow(() => instant1.round({ smallestUnit: "auto" }), RangeError);
shouldThrow(() => time1.round({ smallestUnit: "auto" }), RangeError);
shouldThrow(() => dateTime1.round({ smallestUnit: "auto" }), RangeError);
shouldThrow(() => zoned1.round({ smallestUnit: "auto" }), RangeError);
shouldThrow(() => duration.round({ smallestUnit: "auto" }), RangeError);

// round with string shorthand
shouldThrow(() => instant1.round("auto"), RangeError);
shouldThrow(() => time1.round("auto"), RangeError);
shouldThrow(() => dateTime1.round("auto"), RangeError);
shouldThrow(() => zoned1.round("auto"), RangeError);
shouldThrow(() => duration.round("auto"), RangeError);

// until / since (GetDifferenceSettings)
shouldThrow(() => instant1.until(instant2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => instant1.since(instant2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => time1.until(time2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => time1.since(time2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => date1.until(date2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => date1.since(date2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => dateTime1.until(dateTime2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => dateTime1.since(dateTime2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => yearMonth1.until(yearMonth2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => yearMonth1.since(yearMonth2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => zoned1.until(zoned2, { smallestUnit: "auto" }), RangeError);
shouldThrow(() => zoned1.since(zoned2, { smallestUnit: "auto" }), RangeError);

// toString
shouldThrow(() => instant1.toString({ smallestUnit: "auto" }), RangeError);
shouldThrow(() => time1.toString({ smallestUnit: "auto" }), RangeError);
shouldThrow(() => dateTime1.toString({ smallestUnit: "auto" }), RangeError);
shouldThrow(() => zoned1.toString({ smallestUnit: "auto" }), RangeError);
shouldThrow(() => duration.toString({ smallestUnit: "auto" }), RangeError);

// largestUnit: "auto" remains valid where the spec allows it.
shouldBe(instant1.until(instant2, { largestUnit: "auto" }).toString(), "PT3600S");
shouldBe(time1.until(time2, { largestUnit: "auto" }).toString(), "PT1H");
shouldBe(date1.until(date2, { largestUnit: "auto" }).toString(), "P428D");
shouldBe(duration.round({ largestUnit: "auto" }).toString(), "PT1H");

// largestUnit: "auto" is rejected where the spec does not allow it.
shouldThrow(() => duration.total({ unit: "auto" }), RangeError);
