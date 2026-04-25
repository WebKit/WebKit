function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(Intl.DateTimeFormat().resolvedOptions().timeZone, "America/Los_Angeles");
