//@ requireOptions("--useArrayAllocationProfiling=false")
//@ defaultNoEagerRun

load("./resources/integrity-level-indexed-properties.js", "caller relative");

testIntegrityLevel(false);
