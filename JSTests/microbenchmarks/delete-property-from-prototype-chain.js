//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function assert(b) {
    if (!b)
        throw new Error;
}
noInline(assert)

function getZ(o) {
    return o.z
}
noInline(getZ)

function C() {
    this.x = 42;
}

let objs = [];
for (let i = 0; i < 50; ++i) {
    objs.push(new C);
}

function doTest(zVal) {
    for (let i = 0; i < objs.length; ++i) {
        let o = objs[i];
        assert(o.x === 42);
        assert(getZ(o) === zVal)
    }
}
noInline(doTest);

for (let i=0; i<10000; ++i) {
    const X = { i }
    C.prototype.z = X
    doTest(X)
}

delete C.prototype.z

for (let i=0; i<1000000; ++i) {
    getZ({z: i})
    doTest(undefined)
}

