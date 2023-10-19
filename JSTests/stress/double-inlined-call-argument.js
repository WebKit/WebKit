function foo(a, b, c, p) {
    a = b + c;
    if (p) {
        a++;
        return a;
    }
}

function bar(a, b) {
    return foo("hello", a, b, a <= b);
}

noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var result = bar(2000000000, 2000000000.5);
    if (result != 4000000001.5)
        throw new Error(`Bad result: ${result}!`);
}

if (reoptimizationRetryCount(bar) != 0)
    throw new Error(`Should not have reoptimized bar, but reoptimized ${reoptimizationRetryCount(bar)} times.`);
