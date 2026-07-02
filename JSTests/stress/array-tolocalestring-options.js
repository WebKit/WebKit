function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe([0, 1, 2, 3, 4].toLocaleString('ja-JP', { numberingSystem: 'hanidec' }), `〇,一,二,三,四`);
shouldBe([0, 1, 2, 3, new Date(0)].toLocaleString('en', { timeZone: 'UTC', hour: 'numeric', minute: '2-digit', numberingSystem: 'hanidec' }), `〇,一,二,三,一二:〇〇 AM`);
