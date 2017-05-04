function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let functions = [
    'sin',
    'sinh',
    'cos',
    'cosh',
    'tan',
    'tanh',
    'asin',
    'asinh',
    'acos',
    'acosh',
    'atan',
    'atanh',
    'log',
    'log10',
    'log1p',
    'log2',
    'cbrt',
    'exp',
    'expm1'
];

let repository = {};
for (let func of functions) {
    let wrap = new Function(`return Math.${func}()`);
    noInline(wrap);
    repository[func] = wrap;
}

for (let i = 0; i < 1e4; ++i) {
    for (let func of functions)
        shouldBe(Number.isNaN(repository[func]()), true);
}
