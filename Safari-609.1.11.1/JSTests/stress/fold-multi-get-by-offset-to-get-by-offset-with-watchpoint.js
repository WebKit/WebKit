//@ defaultNoEagerRun

function foo(o) {
    return o.f;
}

for (var i = 0; i < 100; ++i) {
    var result = foo((i & 1) ? {f:1, g:2} : {g:1, f:2});
    if (result != 2 - (i & 1))
        throw "Error: bad result in warm-up loop for i = " + i + ": " + result;
}

function bar(o) {
    return o.g + effectful42() + foo(o);
}

noInline(bar);

for (var i = 0; i < 100000; ++i) {
    var result = bar({f:i, g:i * 3});
    if (result != i * 4 + 42)
        throw "Error: bad result for i = " + i + ": " + result;
}

if (reoptimizationRetryCount(bar))
    throw "Error: reoptimized bar unexpectedly: " + reoptimizationRetryCount(bar);
