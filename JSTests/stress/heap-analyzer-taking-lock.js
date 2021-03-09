//@ requireOptions("--verifyGC=false")
// FIXME: this test intermittently fails --verifyGC. Skipping this test for --verifyGC
// until we have found the root cause and fixed the issue.
// https://bugs.webkit.org/show_bug.cgi?id=222947

// Should not crash.
let array = [];
for (let i = 0; i < 10000; i++) {
    array[i] = new DataView(new ArrayBuffer());
}
for (let i = 0; i < 1000; i++) {}

generateHeapSnapshotForGCDebugging();
