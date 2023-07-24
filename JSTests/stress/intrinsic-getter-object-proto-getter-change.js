//@ requireOptions("--useConcurrentJIT=0", "--jitPolicyScale=0.1", "--useDFGJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function getter() { return {}.__proto__ }
noDFG(getter)

function test() {
    for (var i = 0; i < 1e5; i++)
        shouldBe(getter(), Object.prototype)

    var expectedPrototype = {};

    Object.defineProperty(Object.prototype, "__proto__", {
        get: () => expectedPrototype,
    })

    shouldBe(getter(), expectedPrototype);
}

noDFG(test)
test()
