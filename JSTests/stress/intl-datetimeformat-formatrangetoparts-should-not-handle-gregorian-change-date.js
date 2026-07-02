// Copyright 2019 the V8 project authors. All rights reserved.
// Copyright 2020-2021 Apple Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license.

function shouldBe(actual, expected) {
    // Tolerate different space characters used by different ICU versions.
    // Older ICU uses U+2009 Thin Space in ranges, whereas newer ICU uses
    // regular old U+0020. Let's ignore these differences.
    if (typeof actual === 'string')
        actual = actual.replaceAll(' ', ' ');

    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function compareParts(actual, expected, actualSourceUndefined) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < actual.length; ++i) {
        shouldBe(actual[i].type, expected[i].type);
        shouldBe(actual[i].value, expected[i].value);
        if (actualSourceUndefined)
            shouldBe(actual[i].source, undefined);
    }
}

if ($vm.icuHeaderVersion() >= 67) {
    let df = new Intl.DateTimeFormat("en");
    // A modern date
    let d1 = new Date(2019, 3, 4);
    let d2 = new Date(2019, 4, 5);
    compareParts(df.formatToParts(d1), df.formatRangeToParts(d1, d1), true);
    compareParts(df.formatToParts(d2), df.formatRangeToParts(d2, d2), true);
    compareParts(df.formatRangeToParts(d1, d2), [
        {"type":"month","value":"4","source":"startRange"},
        {"type":"literal","value":"/","source":"startRange"},
        {"type":"day","value":"4","source":"startRange"},
        {"type":"literal","value":"/","source":"startRange"},
        {"type":"year","value":"2019","source":"startRange"},
        {"type":"literal","value":" – ","source":"shared"},
        {"type":"month","value":"5","source":"endRange"},
        {"type":"literal","value":"/","source":"endRange"},
        {"type":"day","value":"5","source":"endRange"},
        {"type":"literal","value":"/","source":"endRange"},
        {"type":"year","value":"2019","source":"endRange"},
    ]);
    // An old date just before the Julian / Gregorian switch
    let d3 = new Date(1582, 8, 13);
    let d4 = new Date(1582, 9, 14);
    compareParts(df.formatToParts(d3), df.formatRangeToParts(d3, d3), true);
    compareParts(df.formatToParts(d4), df.formatRangeToParts(d4, d4), true);
    compareParts(df.formatRangeToParts(d3, d4), [
        {"type":"month","value":"9","source":"startRange"},
        {"type":"literal","value":"/","source":"startRange"},
        {"type":"day","value":"13","source":"startRange"},
        {"type":"literal","value":"/","source":"startRange"},
        {"type":"year","value":"1582","source":"startRange"},
        {"type":"literal","value":" – ","source":"shared"},
        {"type":"month","value":"10","source":"endRange"},
        {"type":"literal","value":"/","source":"endRange"},
        {"type":"day","value":"14","source":"endRange"},
        {"type":"literal","value":"/","source":"endRange"},
        {"type":"year","value":"1582","source":"endRange"},
    ]);
    // An older date
    let d5 = new Date(1000, 0, 1);
    let d6 = new Date(1001, 1, 2);
    compareParts(df.formatToParts(d5), df.formatRangeToParts(d5, d5), true);
    compareParts(df.formatToParts(d6), df.formatRangeToParts(d6, d6), true);
    compareParts(df.formatRangeToParts(d5, d6), [
        {"type":"month","value":"1","source":"startRange"},
        {"type":"literal","value":"/","source":"startRange"},
        {"type":"day","value":"1","source":"startRange"},
        {"type":"literal","value":"/","source":"startRange"},
        {"type":"year","value":"1000","source":"startRange"},
        {"type":"literal","value":" – ","source":"shared"},
        {"type":"month","value":"2","source":"endRange"},
        {"type":"literal","value":"/","source":"endRange"},
        {"type":"day","value":"2","source":"endRange"},
        {"type":"literal","value":"/","source":"endRange"},
        {"type":"year","value":"1001","source":"endRange"},
    ]);
}
