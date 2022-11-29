//@ skip if $model == "Apple Watch Series 3"
//@ $skipModes << :lockdown if ($buildType == "debug") or ($architecture == "mips")

function assert(condition) {
    if (!condition)
        throw new Error("assertion failed")
}
noInline(assert)

function blackbox(x) {
    return x
}
noInline(blackbox)

function doAlloc1() {
    let obj = {}
    obj.x = 5
    obj.y = 7
    obj.y = blackbox(obj.y)
    assert(delete obj.x)
    return obj.y
}
noInline(doAlloc1)

for (let i = 0; i < 50000000; ++i) {
    doAlloc1()
}
