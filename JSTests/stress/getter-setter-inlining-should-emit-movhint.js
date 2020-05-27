//@ skip if not $jitTests
//@ runDefault("--useRandomizingFuzzerAgent=1", "--usePolymorphicCallInliningForNonStubStatus=1", "--seedOfRandomizingFuzzerAgent=2896922505", "--useLLInt=0", "--useConcurrentJIT=0")

function foo(o) {
    o.f = 0;
    return o.f;
}
noInline(foo);

let counter = 0;

function test(o, value) {
    var result = foo(o);
    if (result < value)
        throw new Error(result);
    if (counter < value)
        throw new Error(counter);
    Array.of(arguments);
}

for (var i = 0; i < 100000; ++i) {
    var o = {
        get f() {
            return o
        },
        set f(v) {
            counter++;
            this.z = 0;
        }
    };
    test(o, i, i);
}
