function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// "Etc/Unknown" is ICU's special sentinel for "no specific zone known".
// ucal_getIanaTimeZoneID returns U_ILLEGAL_ARGUMENT_ERROR for it, and the
// fallback canonical form is not in our IANA primary index, so the host TZ
// resolves to UTC via TimeZone's default constructor.
shouldBe(Intl.DateTimeFormat().resolvedOptions().timeZone, `UTC`);
shouldBe(Temporal.Now.timeZoneId(), `UTC`);
