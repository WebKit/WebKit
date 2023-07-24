//@ requireOptions("--useConcurrentJIT=0", "--jitPolicyScale=0.1", "--useDFGJIT=0")

Object.defineProperty(Int8Array.prototype.__proto__, "length", {
    set() {},
})

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

function getter() { return (new Int8Array(10)).length }
noDFG(getter)

function test() {
    for (var i = 0; i < 1e5; i++)
        shouldBe(getter(), 10)

    Object.defineProperty(Int8Array.prototype.__proto__, "length", {
        get() { return 42 },
    })

    shouldBe(getter(), 42)
}

noDFG(test)
test()
