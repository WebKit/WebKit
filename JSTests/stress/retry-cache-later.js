//@ skip if not $jitTests
//@ runNoCJIT("--useLLInt=false", "--useDFGJIT=false")

function foo(o) {
    return o.i7;
}

var o = {};
for (var i = 0; i < 100; ++i)
    o["i" + i] = i;
for (var i = 0; i < 100; i+=2)
    delete o["i" + i];

for (var i = 0; i < 100; ++i) {
    var result = foo(o);
    if (result != 7)
        throw "Error: bad result: " + result;
}

