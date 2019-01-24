// We only need one run of this with any GC or JIT strategy. This test is not particularly fast.
// Unfortunately, it needs to run for a while to test the thing it's testing.
//@ if $architecture =~ /arm|mips/ then skip else runWithRAMSize(10000000) end
//@ slow!

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
if (result > 1000000)
    throw "Error: heap too big after forced GC: " + result;
