function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// Non-IANA legacy three-letter aliases (ACT, AET, AGT, ...) are accepted as
// host time zones via ICU but must canonicalize to their IANA primary so
// Intl.supportedValuesOf("timeZone") and resolvedOptions().timeZone agree.
shouldBe(Intl.DateTimeFormat().resolvedOptions().timeZone, `Australia/Darwin`);
shouldBe(Temporal.Now.timeZoneId(), `Australia/Darwin`);
