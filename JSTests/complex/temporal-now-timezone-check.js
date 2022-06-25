function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(Temporal.Now.timeZone().id, `America/Los_Angeles`);
