function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(JSON.stringify(), undefined);
shouldBe(JSON.stringify(undefined), undefined);
