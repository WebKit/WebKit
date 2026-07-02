//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(msg + ": expected " + JSON.stringify(expected) + ", got " + JSON.stringify(actual));
}

const zdt1 = new Temporal.ZonedDateTime(0n, "+0530");
shouldBe(zdt1.timeZoneId, "+05:30", "'+0530' canonicalized to '+05:30'");

const zdt2 = new Temporal.ZonedDateTime(0n, "-0800");
shouldBe(zdt2.timeZoneId, "-08:00", "'-0800' canonicalized to '-08:00'");

const zdt3 = new Temporal.ZonedDateTime(0n, "+00:00");
shouldBe(zdt3.timeZoneId, "+00:00", "'+00:00' stays '+00:00'");

const zdt4 = new Temporal.ZonedDateTime(0n, "Africa/CAIRO");
shouldBe(zdt4.timeZoneId, "Africa/Cairo", "'Africa/CAIRO' case-normalized to 'Africa/Cairo'");

const zdt5 = new Temporal.ZonedDateTime(0n, "Asia/Calcutta");
shouldBe(zdt5.timeZoneId, "Asia/Calcutta", "'Asia/Calcutta' alias preserved");

const zdt6 = new Temporal.ZonedDateTime(0n, "+05:30");
shouldBe(zdt6.timeZoneId, "+05:30", "'+05:30' stays '+05:30'");

const zdt7 = new Temporal.ZonedDateTime(0n, "UTC");
shouldBe(zdt7.timeZoneId, "UTC", "'UTC' stays 'UTC'");
