function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error('bad value: ' + String(actual) + ' expected: ' + String(expected));
}

var cases = [
    ['', 0], ['   ', 0], [' 12 ', 12], ['.5', 0.5], ['+1', 1], ['-1', -1], ['-0', -0],
    ['Infinity', Infinity], ['+Infinity', Infinity], ['-Infinity', -Infinity], [' Infinity ', Infinity],
    ['infinity', NaN], ['I', NaN], ['Inf', NaN], ['0x10', 16], ['0b11', 3], ['0o17', 15],
    ['e5', NaN], ['rlineto', NaN], ['\u00a05', 5], ['\u30001', 1], ['\u0661', NaN], ['NaN', NaN],
    ['1e3', 1000], ['.', NaN], ['+', NaN], ['-', NaN], ['_1', NaN], ['\ufeff7', 7],
];

function toNumber(value) { return +value; }
noInline(toNumber);
function looseEqual(a, b) { return a == b; }
noInline(looseEqual);

for (var i = 0; i < testLoopCount; ++i) {
    var [string, expected] = cases[i % cases.length];
    shouldBe(toNumber(string), expected);
    shouldBe(looseEqual(string, -1), expected === -1);
    shouldBe(Number(string), expected);
}
