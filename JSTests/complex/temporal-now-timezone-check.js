function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(Temporal.Now.timeZoneId(), `America/Los_Angeles`);
