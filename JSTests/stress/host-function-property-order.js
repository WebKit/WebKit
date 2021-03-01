function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(JSON.stringify(Object.getOwnPropertyNames(Object.getOwnPropertyNames)), `["length","name"]`);
