function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// UTC-equivalent host time zone must normalize to "UTC", not "Etc/UTC" or
// the literal alias name. ECMA-402 mandates "UTC" as the primary identifier
// for UTC-equivalent zones.
shouldBe(Intl.DateTimeFormat().resolvedOptions().timeZone, `UTC`);
shouldBe(Temporal.Now.timeZoneId(), `UTC`);
