//@ runDefault("--useConcurrentJIT=0", "--validateFTLOSRExitLiveness=1")

function foo(o, p) {
    p = null;
    try {
        o.f = null;
        p = null;
    } catch (e) {
    }
}
noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    foo({});
}
