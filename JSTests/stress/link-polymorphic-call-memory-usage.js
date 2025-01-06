// Run this with --memory-limited!
//
// This tests for a bug we had on armv7 where we would wrongly re-link
// polymorphic calls every time they are called.

samples = []
for (i=0; i <= 1e6; i++) {
    samples[i] = i
}
for (i=1; i <= 20; i++) {
    //print("ITERATION", i)
    //$vm.gc()
    samples.forEach(function () {})
}
