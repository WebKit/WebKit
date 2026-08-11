//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0.1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function opt(){
    let A = [];

    for (let i = 0; i < 500; ++i) {
      A[i]=500
    }
    A.shift()

    let count = 0
    for (let j in A) {
      A.shift()
      count++
    }
    return count
}
for(let ii=0;ii<1000;ii++){
  opt()
}

shouldBe(opt(), 250);
