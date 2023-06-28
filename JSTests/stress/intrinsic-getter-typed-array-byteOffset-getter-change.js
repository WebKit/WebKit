//@ requireOptions("--useConcurrentJIT=0", "--jitPolicyScale=0.1", "--useDFGJIT=0")

Object.defineProperty(Uint8Array.prototype.__proto__, "byteOffset", {
    set() {},
})

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

var arr = new Uint8Array(10)
function getter() { return arr.byteOffset }
noDFG(getter)

function test() {
    for (var i = 0; i < 1e5; i++)
        shouldBe(getter(), 0)

    Object.defineProperty(Uint8Array.prototype.__proto__, "byteOffset", {
        get: function() { return 42 },
    })

    shouldBe(getter(), 42)
}

noDFG(test)
test()
