function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// IANA Backward link must collapse to the IANA primary identifier when used
// as the host time zone. https://bugs.webkit.org/show_bug.cgi?id=310866
shouldBe(Intl.DateTimeFormat().resolvedOptions().timeZone, `Asia/Kolkata`);
shouldBe(Temporal.Now.timeZoneId(), `Asia/Kolkata`);
