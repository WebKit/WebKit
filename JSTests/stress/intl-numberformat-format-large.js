function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + " " + expected);
}

const nf = new Intl.NumberFormat("en-US");
const nf2 = new Intl.NumberFormat("ja-JP");
const start = 987654321987654321n;
const end = 987654321987654322n;
shouldBe(nf.format(start), `987,654,321,987,654,321`);
shouldBe(nf2.format(start), `987,654,321,987,654,321`);
if (nf.formatRange) {
    shouldBe(nf.formatRange(start, end), `987,654,321,987,654,321–987,654,321,987,654,322`);
    shouldBe(nf2.formatRange(start, end), `987,654,321,987,654,321～987,654,321,987,654,322`);
}
