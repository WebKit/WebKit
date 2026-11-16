//@ runDefault("--useConcurrentJIT=0", "--validateAbstractInterpreterStateProbability=1.0", "--validateAbstractInterpreterState=1")

function foo(a, v) {
    a[0] = v + 2000000000;
}
noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    foo({}, 1000000000);
}
