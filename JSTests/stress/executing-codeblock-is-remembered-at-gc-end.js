//@ runDefault("--verifyHeap=1", "--sweepSynchronously=1", "--useDFGJIT=false")

// test() is on the stack across the Eden collection inside it, and keeps recording fresh strings in its
// value profile afterwards without passing op_enter again. The next Eden collection has to reconcile that
// profile, or the profile keeps a string the collection frees and the heap verifier reports it.

function test(n) {
    let result = 0;
    for (let i = 0; i < n; ++i) {
        let object = { p: "s".repeat(40) + i + Math.random() };
        let value = object.p;
        result += value.length;
        if (i == 2)
            edenGC();
    }
    return result;
}
noInline(test);

test(1);
fullGC();
test(10);
edenGC();
edenGC();
