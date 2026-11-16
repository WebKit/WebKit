function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected: " + JSON.stringify(expected) + ", actual: " + JSON.stringify(actual));
}

function shouldBeArray(actual, expected, message) {
    if (actual === null && expected !== null)
        throw new Error((message ? message + ": " : "") + "expected: " + JSON.stringify(expected) + ", actual: null");
    if (actual === null || expected === null) {
        if (actual !== expected)
            throw new Error((message ? message + ": " : "") + "expected: " + JSON.stringify(expected) + ", actual: " + JSON.stringify(actual));
        return;
    }
    if (actual.length !== expected.length)
        throw new Error((message ? message + ": " : "") + "expected length: " + expected.length + ", actual length: " + actual.length);
    for (let i = 0; i < expected.length; i++) {
        if (actual[i] !== expected[i])
            throw new Error((message ? message + ": " : "") + "at index " + i + ", expected: " + JSON.stringify(expected[i]) + ", actual: " + JSON.stringify(actual[i]));
    }
}

(function testBackreferenceWithCaptureReset() {
    let re = /(?:^(a)|\1(a)|(ab)){2}/;
    let str = 'aab';
    let actual = re.exec(str);

    shouldBeArray(actual, ['aa', undefined, 'a', undefined], "backreference with capture reset");
    shouldBe(actual.index, 0, "match index");
    shouldBe(actual.input, str, "input string");
})();

(function testDuplicateNamedGroupsWithFixedCount() {
    let result1 = /(?:(?:(?<a>x)|(?<a>y))\k<a>){2}/.exec('xxyy');
    shouldBeArray(result1, ['xxyy', undefined, 'y'], "duplicate named groups - simple case");

    let result2 = /(?:(?:(?<a>x)|(?<a>y)|(a)|(?<b>b)|(?<a>z))\k<a>){3}/.exec('xzzyyxxy');
    shouldBeArray(result2, ['zzyyxx', 'x', undefined, undefined, undefined, undefined], "duplicate named groups - complex case");

    let result3 = 'xxyy'.match(/(?:(?:(?<a>x)|(?<a>y))\k<a>){2}/);
    shouldBeArray(result3, ['xxyy', undefined, 'y'], "duplicate named groups - String.match");
})();

(function testCaptureResetInVariousPatterns() {
    let re1 = /(?:(a)|\1b){2}/;
    let result1 = re1.exec('ab');
    shouldBeArray(result1, ['ab', undefined], "backreference reset to empty string");

    let re2 = /(?:(a)|b){2}/;
    let result2 = re2.exec('ab');
    shouldBeArray(result2, ['ab', undefined], "capture reset - alternation without backreference");

    let re3 = /(?:(a)|b){3}/;
    let result3 = re3.exec('aba');
    shouldBeArray(result3, ['aba', 'a'], "capture reset - three iterations ending with capture");

    let result4 = re3.exec('abb');
    shouldBeArray(result4, ['abb', undefined], "capture reset - three iterations ending without capture");
})();

(function testNestedNonCapturingWithCaptures() {
    let re = /(?:(a)(b)){2}/;
    let result = re.exec('abab');
    shouldBeArray(result, ['abab', 'a', 'b'], "nested capturing groups in fixed count");
})();

(function testNonCapturingWithoutNestedCaptures() {
    let re = /(?:ab){3}/;
    let result = re.exec('ababab');
    shouldBeArray(result, ['ababab'], "non-capturing without nested captures");

    let re2 = /(?:a|b){4}/;
    let result2 = re2.exec('abba');
    shouldBeArray(result2, ['abba'], "non-capturing alternation without captures");
})();
