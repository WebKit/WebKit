//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function assert(b) {
    if (!b)
        throw new Error;
}

function C() {
    this.x = 42;
    this.y = 1337;
    delete this.y;
}

let objs = [];
for (let i = 0; i < 50; ++i) {
    objs.push(new C);
}

function doTest() {
    for (let i = 0; i < objs.length; ++i) {
        let o = objs[i];
        assert(o.y === undefined);
        assert(o.x === 42);
    }
}
noInline(doTest);

for (let i=0; i<10000000; ++i) doTest()

