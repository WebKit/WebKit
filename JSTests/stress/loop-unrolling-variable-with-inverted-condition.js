//@ runDefault("--usePartialLoopUnrolling=1", "--useConcurrentJIT=1")
function test(results, limit)
{
    var i = -2147483648 + 10;
    do {
        results.push(Math.abs(i));
        --i;
    } while (i !== limit);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    var results = [];
    test(results, -2147483647);
    if (results.length != 9)
        throw "Wrong result length: " + results.length;
    for (var j = 0; j < 9; ++j) {
        if (results[j] !== 2147483638 + j)
            throw "Wrong result, results[j] = " + results[j] + " at j = " + j;
    }
}
