function doMod(a, b) {
    return a % b;
}
noInline(doMod);

// Use + 0.0 to ensure double representation while keeping integer values.
var result = 0;
for (var i = 0; i < 5000000; ++i) {
    var divisor = 7.0;
    if (i % 10 == 0)
        divisor = 7.5;
    result = (result + doMod((i + 1) + 0.0, divisor)) | 0;
}

if (result !== 15166668)
    throw "Bad result: " + result;
