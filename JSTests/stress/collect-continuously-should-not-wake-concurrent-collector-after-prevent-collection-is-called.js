//@ runDefault("--maxPerThreadStackUsage=1572864", "--forceMiniVMMode=1", "--stealEmptyBlocksFromOtherAllocators=0", "--collectContinuously=1", "--watchdog=3000", "--watchdog-exception-ok", "--verboseHeapSnapshotLogging=0")
// Reproducing the crash with this test is very hard. You should execute something like this.
// while true; do for x in {0..4}; do DYLD_FRAMEWORK_PATH=$VM $VM/jsc --maxPerThreadStackUsage=1572864 --forceMiniVMMode=1 --stealEmptyBlocksFromOtherAllocators=0 --collectContinuously=1 collect-continuously-should-not-wake-concurrent-collector-after-prevent-collection-is-called.js --watchdog=3000&; done; wait; sleep 0.1; done

array = [];
for (var i = 0; i < 800; ++i) {
  array[i] = new DataView(new ArrayBuffer());
}

let theCode = `
for (let j = 0; j < 100; j++) {
  generateHeapSnapshotForGCDebugging();
}
`

for (let i=0; i<5; i++) {
  $.agent.start(theCode);
}

for (let i=0; i<3; i++) {
  runString(theCode);
}
