//@ runDefault("--useBigInt=true", "--useConcurrentJIT=false")

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function foo(o) {
    let newString = o.toString();
    if (typeof o === "bigint")
        return newString;
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    assert(foo(3n) === "3");
}

