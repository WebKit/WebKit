function firstCareAboutZeroSecondDoesNot(a) {
    var resultA = Math.round(a);
    var resultB = Math.round(a)|0;
    return { resultA:resultA, resultB:resultB };
}
noInline(firstCareAboutZeroSecondDoesNot);

function firstDoNotCareAboutZeroSecondDoes(a) {
    var resultA = Math.round(a)|0;
    var resultB = Math.round(a);
    return { resultA:resultA, resultB:resultB };
}
noInline(firstDoNotCareAboutZeroSecondDoes);

// Warmup with doubles, but nothing that would round to -0 to ensure we never
// see a double as result. The result must be integers, the input is kept to small values.
function warmup() {
    for (var i = 0; i < 1e4; ++i) {
        firstCareAboutZeroSecondDoesNot(42.6 + i);
        firstDoNotCareAboutZeroSecondDoes(42.4 + i);
    }
}
warmup();

function verifyNegativeZeroIsPreserved() {
    for (var i = 0; i < 1e4; ++i) {
        var result1 = firstCareAboutZeroSecondDoesNot(-0.1);
        if (1 / result1.resultA !== -Infinity) {
            throw "Failed firstCareAboutZeroSecondDoesNot(-0.1), resultA = " + result1.resultA;
        }
        if (1 / result1.resultB !== Infinity) {
            throw "Failed firstCareAboutZeroSecondDoesNot(-0.1), resultB = " + result1.resultB;
        }
        var result2 = firstDoNotCareAboutZeroSecondDoes(-0.1);
        if (1 / result2.resultA !== Infinity) {
            throw "Failed firstDoNotCareAboutZeroSecondDoes(-0.1), resultA = " + result1.resultA;
        }
        if (1 / result2.resultB !== -Infinity) {
            throw "Failed firstDoNotCareAboutZeroSecondDoes(-0.1), resultB = " + result1.resultB;
        }

    }
}
verifyNegativeZeroIsPreserved();