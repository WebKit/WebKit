//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function escape(e) {
}
noInline(escape);

function opt() {
  for (let i=0; i<15; i++) {
    const v54 = [];
    Object.freeze(v54);
    try {
      v54.pop();
      escape(1);
      return false;
    } catch (e) {
      escape(e);
    }
  }
  return true;
}

shouldBe(opt(), true);
