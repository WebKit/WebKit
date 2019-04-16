//@ runDefault("--useConcurrentJIT=0", "--validateAbstractInterpreterState=1", "--validateAbstractInterpreterStateProbability=1.0", "--forceEagerCompilation=1")

function foo(a, v) {
    a[0] = v + 2000000000;
}
noInline(foo);

for (var i = 0; i < 100000; ++i) {
    foo({}, 1000000000);
}
