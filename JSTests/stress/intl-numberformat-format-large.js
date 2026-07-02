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
if (nf.formatRangeToParts) {
    shouldBe(JSON.stringify(nf.formatRangeToParts(0, 30000)), `[{"type":"integer","value":"0","source":"startRange"},{"type":"literal","value":"–","source":"shared"},{"type":"integer","value":"30","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"000","source":"endRange"}]`);
    shouldBe(JSON.stringify(nf.formatRangeToParts(start, end)), `[{"type":"integer","value":"987","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"654","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"321","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"987","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"654","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"321","source":"startRange"},{"type":"literal","value":"–","source":"shared"},{"type":"integer","value":"987","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"654","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"321","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"987","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"654","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"322","source":"endRange"}]`);
    shouldBe(JSON.stringify(nf2.formatRangeToParts(start, end)), `[{"type":"integer","value":"987","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"654","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"321","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"987","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"654","source":"startRange"},{"type":"group","value":",","source":"startRange"},{"type":"integer","value":"321","source":"startRange"},{"type":"literal","value":"～","source":"shared"},{"type":"integer","value":"987","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"654","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"321","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"987","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"654","source":"endRange"},{"type":"group","value":",","source":"endRange"},{"type":"integer","value":"322","source":"endRange"}]`);
}
