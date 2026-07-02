//@ runDefault("--useOSRExitFuzz=1", "--fireOSRExitFuzzAtOrAfter=1", "--jitPolicyScale=0", "--useConcurrentJIT=0")
const ta = new Uint8Array();
function foo() {
  function bar() {
    ta[0] = 0;
  }
  for (let i=0; i<testLoopCount; i++) {
    bar();
  }

}
foo();
