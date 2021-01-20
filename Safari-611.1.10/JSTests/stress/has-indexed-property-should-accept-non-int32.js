//@ runDefault("--useRandomizingFuzzerAgent=1", "--jitPolicyScale=0", "--useConcurrentJIT=0")
function foo(obj) {
  for (var x in obj) {
    if (0 > 0) {
      break;
    }
  }
  0 && Object.prototype.hasOwnProperty
}

foo([]);
foo([]);
foo([0, 0]);
