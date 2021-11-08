function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + " " + expected);
}

const nf = new Intl.NumberFormat("en-US");
const nf2 = new Intl.NumberFormat("ja-JP");
shouldBe(nf.format("54.321"), `54.321`);
if (nf.formatRange) {
    shouldBe(nf.formatRange("-54.321", "+54.321"), `-54.321 – 54.321`);
    shouldBe(nf.formatRange("-54.321", "20000000000000000000000000000000000000000"), `-54.321 – 20,000,000,000,000,000,000,000,000,000,000,000,000,000`);
    shouldBe(nf.formatRange("-0", "0"), `-0 – 0`);
    shouldBe(nf.formatRange("-0", "1000000000000000000000000000000000000000000000"), `-0 – 1,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000`);
    shouldBe(nf2.formatRange("-54.321", "+54.321"), `-54.321 ～ 54.321`);
    shouldBe(nf2.formatRange("-54.321", "20000000000000000000000000000000000000000"), `-54.321 ～ 20,000,000,000,000,000,000,000,000,000,000,000,000,000`);
    shouldBe(nf2.formatRange("-0", "0"), `-0 ～ 0`);
    shouldBe(nf2.formatRange("-0", "1000000000000000000000000000000000000000000000"), `-0 ～ 1,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000`);
}
