//@ skip if not $jitTests
//@ runDefault("--useConcurrentJIT=false")

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function foo() {
    return typeof this;
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    assert(foo.apply(10n) === "object");
}

for (let i = 0; i < 10000; i++) {
    assert(foo.apply(300) === "object");
}

assert(numberOfDFGCompiles(foo) === 1);

