//@ runDefault("--useJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function llintCode1() {
    return $vm.llintTrue();
}

function llintCode2() {
    return $vm.baselineJITTrue();
}

shouldBe(llintCode1(), true);
shouldBe(llintCode2(), false);
