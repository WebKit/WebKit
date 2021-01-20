//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0.1")

function test() {
    var [w, y] = [false, 0, null];
    if (w != y)
        throw 0;
}
noInline(test);

for (var i = 0; i < 2000; ++i)
    test();
