//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0", "--maximumInliningDepth=2")

function foo(x, y) {
    var w = 0;
    for (var i = 0; i < x.length; ++i) {
        for (var j = 0; j < x.length; ++j)
            w += foo(j, i);
        y[i] = w;
    }
}

function test(x, a3) {
      a1 = [];
      a2 = [];

    for (i = 0; i < x; ++i)
        a1[i] = 0;

    for (i = 0; i < 10; ++i) {
        foo(a3, a2);
        foo(a3, a1);
    }
}
noDFG(test);

a3 = [];
for (var i = 0; i < 3; ++i)
    a3[i] = 0;

for (var i = 3; i <= 12; i *= 2)
    test(i, a3);
