//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function f1(o, value) {
    function f2()
    {
        o.x=value
        return 2
    }
    let y={}
    y.toString = f2
    y >>> 1;
  }

  noInline(f1)
  let obj={}
  for (let v25 = 0; v25 < 100; v25++) {
    f1(obj, v25);

  }
shouldBe(obj.x, 99)
