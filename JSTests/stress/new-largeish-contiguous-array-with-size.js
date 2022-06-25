// We only need one run of this with any GC or JIT strategy. This test is not particularly fast.
// Unfortunately, it needs to run for a while to test the thing it's testing.
//@ requireOptions("-e", "let leakFactor=3") if $architecture == "mips"
//@ runWithRAMSize(10000000)
//@ slow!

// Note: Due to the conservative stack scanning, it is possible that arbitrary
// values on a thread stack allocated between thread startup and entering the
// user defined thread entry point alias heap cells (objects), which can
// therefore never be collected. Currently this manifests on some of the MIPS
// test targets, with the main thread, where cruft on the main thread stack
// allocated by the OS and crt0 during process startup aliases some of the
// Arrays allocated in the loop. This would then cause the final check on the
// post GC heap size to fail, as some of the arrays cannot be collected, so ...
// increasing leniency on platforms where this manifests.
leakFactor = typeof(leakFactor) === 'undefined' ? 1 : leakFactor;

function foo(x) {
    return new Array(x);
}

noInline(foo);

function test(size) {
    var result = foo(size);
    if (result.length != size)
        throw "Error: bad result: " + result;
    var sawThings = false;
    for (var s in result)
        sawThings = true;
    if (sawThings)
        throw "Error: array is in bad state: " + result;
    result[0] = "42.5";
    if (result[0] != "42.5")
        throw "Error: array is in weird state: " + result;
}

var result = gcHeapSize();

for (var i = 0; i < 1000; ++i) {
    // The test was written when we found that large array allocations weren't being accounted for
    // in that part of the GC's accounting that determined the GC trigger. Consequently, the GC
    // would run too infrequently in this loop and we would use an absurd amount of memory when this
    // loop exited.
    test(50000);
}

// Last time I tested, the heap should be 3725734 before and 125782 after. I don't want to enforce
// exactly that. If you regress the accounting code, the GC heap size at this point will be much
// more than that.
var result = gcHeapSize();
if (result > 10000000)
    throw "Error: heap too big before forced GC: " + result;

// Do a final check after GC, just for sanity.
gc();
result = gcHeapSize();
if (result > 1000000*leakFactor)
    throw "Error: heap too big after forced GC: " + result;
