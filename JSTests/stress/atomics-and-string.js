//@ runDefault("--useRandomizingFuzzerAgent=1", "--useConcurrentJIT=0")

function foo() {
    return Atomics.load('', 0);
}
noInline(foo);

for (let i=0; i<1e4; i++) {
    try {
        foo();
    } catch { }
}
