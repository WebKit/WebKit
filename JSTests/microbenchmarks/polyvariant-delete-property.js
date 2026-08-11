//@ skip if $model == "Apple Watch Series 3"
//@ $skipModes << :lockdown if $buildType == "debug"

function assert(condition) {
    if (!condition)
        throw new Error("assertion failed")
}

function blackbox(x) {
    return x
}
noInline(blackbox)

function polyvariant(x) {
    assert(delete x.x)
}

function doAlloc1() {
    let obj = {}
    obj.x = 5
    obj.y = 7
    obj.y = blackbox(obj.y)
    polyvariant(obj)
    return obj.y
}
noInline(doAlloc1)

function doAlloc2() {
    let obj = {}
    obj.x = 5
    obj.b = 9
    obj.y = 7
    obj.y = blackbox(obj.y)
    polyvariant(obj)
    return obj.y
}
noInline(doAlloc2)

for (let i = 0; i < 10000000; ++i) {
    doAlloc1()
    doAlloc2()
}
