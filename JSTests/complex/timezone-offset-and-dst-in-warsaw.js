function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe((new Date(1969, 8, 26)).toLocaleDateString(), `9/26/1969`);
