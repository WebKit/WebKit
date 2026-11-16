function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function parseDecimal(string) { return parseInt(string, 10); }
noInline(parseDecimal);
function parseNoRadix(string) { return parseInt(string); }
noInline(parseNoRadix);
function parseHex(string) { return parseInt(string, 16); }
noInline(parseHex);

function makeRope(parts) {
    var result = '';
    for (var i = 0; i < parts.length; ++i)
        result += parts[i];
    return result;
}

var longDigits = '';
for (var i = 0; i < 100; ++i)
    longDigits += '9';

for (var i = 0; i < testLoopCount; ++i) {
    var rope = makeRope(['1', '2', '3', String(i % 10)]);
    shouldBe(parseDecimal(rope), 1230 + i % 10);
    shouldBe(rope, '123' + (i % 10));
    shouldBe(parseNoRadix(makeRope([' ', '-', '4', '2', 'x'])), -42);
    shouldBe(parseNoRadix(makeRope(['0', 'x', 'f', 'F'])), 255);
    shouldBe(parseHex(makeRope(['f', 'f'])), 255);
    shouldBe(parseDecimal(makeRope(['\u0661', '2'])), NaN);
    shouldBe(parseDecimal(makeRope(['\u3000', '7', '\u2028'])), 7);
    shouldBe(parseDecimal(makeRope(['abc', 'def'])), NaN);
    shouldBe(parseDecimal(makeRope([longDigits, '1'])), parseInt(longDigits + '1', 10));
    shouldBe(parseDecimal(makeRope(['12', '34']).substring(1, 3)), 23);
    shouldBe(parseDecimal(makeRope(['', ''])), NaN);
}
