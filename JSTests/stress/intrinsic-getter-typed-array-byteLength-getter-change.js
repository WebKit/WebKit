//@ requireOptions("--useConcurrentJIT=0", "--jitPolicyScale=0.1", "--useDFGJIT=0")

Object.defineProperty(Float32Array.prototype.__proto__, "byteLength", {
    set() {},
})

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

var arr = new Float32Array(10)
function getter() { return arr.byteLength }
noDFG(getter)

function test() {
    for (var i = 0; i < 1e5; i++)
        shouldBe(getter(), 40)

    Object.defineProperty(Float32Array.prototype.__proto__, "byteLength", {
        get: function() { return 0 },
    })

    shouldBe(getter(), 0)
}

noDFG(test)
test()
