//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function opt(start, end, index) {
  for (let j = start; j <= end; j++) {
    function f() { f = start; }
    shouldBe(typeof j, "number")
  }
}

for (let i = 0; i < 10; i++) {
  opt(opt, i);
  opt();   // print accidently
}
