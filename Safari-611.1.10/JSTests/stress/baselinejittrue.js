//@ runDefault("--useLLInt=0", "--useDFGJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function jitCode1() {
    return $vm.llintTrue();
}

function jitCode2() {
    return $vm.baselineJITTrue();
}

if ($vm.useJIT()) {
    shouldBe(jitCode1(), false);
    shouldBe(jitCode2(), true);
}
