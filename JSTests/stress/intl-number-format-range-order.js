function shouldBe(actual, expected) {
    // Tolerate different space characters used by different ICU versions.
    // Older ICU uses U+2009 Thin Space in ranges, whereas newer ICU uses
    // regular old U+0020. Let's ignore these differences.
    if (typeof actual === 'string')
        actual = actual.replaceAll(' ', ' ');

    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected value: ' + expected);
}

function compareParts(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < actual.length; ++i) {
        shouldBe(actual[i].type, expected[i].type);
        shouldBe(actual[i].value, expected[i].value);
        shouldBe(actual[i].source, expected[i].source);
    }
}

var fmt = new Intl.NumberFormat('en-US');
if (fmt.formatRange) {
    shouldBe(fmt.formatRange(20, -20), `20–-20`);
    shouldBe(fmt.formatRange(0, -0), `0–-0`);
}
if (fmt.formatRangeToParts) {
    compareParts(fmt.formatRangeToParts(20, -20), [
        {"type":"integer","value":"20","source":"startRange"},
        {"type":"literal","value":"–","source":"shared"},
        {"type":"minusSign","value":"-","source":"endRange"},
        {"type":"integer","value":"20","source":"endRange"}
    ]);
    compareParts(fmt.formatRangeToParts(0, -0), [
        {"type":"integer","value":"0","source":"startRange"},
        {"type":"literal","value":"–","source":"shared"},
        {"type":"minusSign","value":"-","source":"endRange"},
        {"type":"integer","value":"0","source":"endRange"}
    ]);
}
