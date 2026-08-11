//@ runDefault("--useConcurrentJIT=false")

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function foo() {
    "use strict";
    return typeof this;
}
noInline(foo);

for (let i = 0; i < testLoopCount; i++) {
    assert(foo.apply(10n) === "bigint");
}

for (let i = 0; i < testLoopCount; i++) {
    assert(foo.apply(300) === "number");
}

assert(numberOfDFGCompiles(foo) > 1);

