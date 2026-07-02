function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe([].toLocaleString(), ``);
shouldBe([42].toLocaleString('en', { numberingSystem: 'hanidec' }), `四二`);
