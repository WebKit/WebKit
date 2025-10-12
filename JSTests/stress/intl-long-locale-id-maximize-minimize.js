function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeOneOf(actual, expectedArray) {
    if (!expectedArray.some((value) => value === actual))
        throw new Error('bad value: ' + actual + ' expected values: ' + expectedArray);
}
{
    const locale = new Intl.Locale(`de-Latn-DE-u-cu-eur-em-default-hc-h23-ks-level1-lb-strict-lw-normal-ms-metric-nu-latn-rg-atzzzz-sd-atat1-ss-none-tz-atvie-va-posix`);
    shouldBe(locale.minimize().toString(), `de-u-cu-eur-em-default-hc-h23-ks-level1-lb-strict-lw-normal-ms-metric-nu-latn-rg-atzzzz-sd-atat1-ss-none-tz-atvie-va-posix`);
}
{
    const locale = new Intl.Locale(`de-u-cu-eur-em-default-hc-h23-ks-level1-lb-strict-lw-normal-ms-metric-nu-latn-rg-atzzzz-sd-atat1-ss-none-tz-atvie-va-posix`);
    shouldBe(locale.maximize().toString(), `de-Latn-DE-u-cu-eur-em-default-hc-h23-ks-level1-lb-strict-lw-normal-ms-metric-nu-latn-rg-atzzzz-sd-atat1-ss-none-tz-atvie-va-posix`);
}
{
    const locale = new Intl.Locale(`de-variant0-rozaj-biske-nedis-variant1-variant2-variant3-variant4-variant5-variant6-variant7-variant8-variant9-varianta-variantb-variantc-variantd-variante-variantf-variantg-varianth-varianti-variantj-variantk`);
    const result = locale.maximize().toString();
    shouldBeOneOf(result, [`de-Latn-DE`, `de`, `en-Latn-US`]);
}
{
    const locale = new Intl.Locale(`de-Latn-DE-rozaj-biske-nedis-variant0-variant1-variant2-variant3-variant4-variant5-variant6-variant7-variant8-variant9-varianta-variantb-variantc-variantd-variante-variantf-variantg-varianth-varianti-variantj-variantk`);
    const result = locale.maximize().toString();
    shouldBeOneOf(result, [`de-Latn-DE`, `de`, `en-Latn-US`]);
}
