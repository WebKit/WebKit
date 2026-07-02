function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(new Intl.NumberFormat(`en-US-u-nu-hanidec`).resolvedOptions().locale, `en-US-u-nu-hanidec`);
shouldBe(new Intl.NumberFormat(`en-US`, { numberingSystem: "hanidec" }).resolvedOptions().locale, `en-US`);
shouldBe(new Intl.NumberFormat(`en-US-u-nu-hanidec`, { numberingSystem: 'latn' }).resolvedOptions().locale, `en-US`);
shouldBe(new Intl.NumberFormat(`en-US-u-nu-latn`, { numberingSystem: 'hanidec' }).resolvedOptions().locale, `en-US`);
shouldBe(new Intl.NumberFormat(`en-US-u-nu-adlm`, { numberingSystem: 'hanidec' }).resolvedOptions().locale, `en-US`);

shouldBe(new Intl.NumberFormat(`en-US-u-nu-hanidec`).format(20), `二〇`);
shouldBe(new Intl.NumberFormat(`en-US`, { numberingSystem: "hanidec" }).format(20), `二〇`);
shouldBe(new Intl.NumberFormat(`en-US-u-nu-hanidec`, { numberingSystem: 'latn' }).format(20), `20`);
shouldBe(new Intl.NumberFormat(`en-US-u-nu-latn`, { numberingSystem: 'hanidec' }).format(20), `二〇`);
shouldBe(new Intl.NumberFormat(`en-US-u-nu-adlm`, { numberingSystem: 'hanidec' }).format(20), `二〇`);
