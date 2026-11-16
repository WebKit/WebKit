//@ runDefault("--thresholdForFTLOptimizeAfterWarmUp=0", "--osrExitCountForReoptimization=15", "--useConcurrentJIT=0")

function main() {
  function v1(v2, v3) {}
  function v4(v5, v6, v7, v8, v9) {
    flashHeapAccess(0.1);
    for (let v14 = 0; v14 < 100; v14++) {
      const v15 = {};
      switch (v5) {
      case v15:
      case v7:
      case `number${-547391.1881778344}boolean${v1}E${v5}icKGfbgqm5`:
      }
    }
  }
  for (let v20 = 0; v20 < 100; v20++) {
    const v21 = v4?.(-2147483647n);
  }
}
main();
