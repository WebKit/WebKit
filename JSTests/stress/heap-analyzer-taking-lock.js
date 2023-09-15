//@ requireOptions("--verboseHeapSnapshotLogging=0")

// Should not crash.
let array = [];
for (let i = 0; i < 10000; i++) {
    array[i] = new DataView(new ArrayBuffer());
}
for (let i = 0; i < 1000; i++) {}

generateHeapSnapshotForGCDebugging();
