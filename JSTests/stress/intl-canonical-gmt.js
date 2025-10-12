function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(new Intl.DateTimeFormat('en', { timeZone: "GMT" }).resolvedOptions().timeZone, "GMT")
