//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function opt() {
    const v1 = Object.getPrototypeOf('').toString;
    try {
        v1.call();
        return 0
    } catch (e) {
        return String(e);
    }
}


shouldBe(opt(), `TypeError: Type error`);
for (let i = 0; i < 10; i++)
    shouldBe(opt(), `TypeError: Type error`);
