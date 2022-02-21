//@ runDefault("--collectContinuously=1", "--useAccessInlining=0", "--verifyGC=1")

function PutByValICPrimitive() {
    let leak = []

    function doByVal(o, s) {
        o[s] = 1337
    }
    noInline(doByVal)

    for (let i = 0; i < 1000000; ++i) {
        let o1 = {a: 1, b: 2}
        let o2 = {a: 1, b: 2}
        let o3 = {a: 1, b: 2}
        doByVal(o1, "x")
        doByVal(o2, "y")
        doByVal(o3, "z")
        leak.push(o1, o2, o3)
    }
}
noInline(PutByValICPrimitive)
PutByValICPrimitive()