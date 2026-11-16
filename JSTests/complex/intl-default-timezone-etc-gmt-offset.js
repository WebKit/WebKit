function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// Etc/GMT+N and Etc/GMT-N (with non-zero N) are real IANA primary zones, not
// aliases, so they must round-trip unchanged. Only Etc/GMT+0 and Etc/GMT-0
// collapse to "UTC" (covered separately by intl-default-timezone-utc-alias.js).
shouldBe(Intl.DateTimeFormat().resolvedOptions().timeZone, `Etc/GMT+5`);
shouldBe(Temporal.Now.timeZoneId(), `Etc/GMT+5`);
