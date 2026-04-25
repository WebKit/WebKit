function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// Country/region aliases like "US/Eastern", "GB", "Brazil/East", "Canada/*"
// are IANA Backward links and must canonicalize to their primary identifier.
shouldBe(Intl.DateTimeFormat().resolvedOptions().timeZone, `America/New_York`);
shouldBe(Temporal.Now.timeZoneId(), `America/New_York`);
