// The main purpose of this test is to ensure that
// we will re-use no longer in use CallSiteIndices for
// inline cache stubs. See relevant code in destructor
// which calls:
// DFG::CommonData::removeCallSiteIndex(.)
// CodeBlock::removeExceptionHandlerForCallSite(.)
// Which add old call site indices to a free list.

function assert(b) {
    if (!b)
        throw new Error("bad value");
}
noInline(assert);

var arr = []
function allocate() {
    for (var i = 0; i < 10000; i++)
        arr.push({});
}

function hello() { return 20; }
noInline(hello);

let __jaz = {};
function jazzy() {
    return __jaz;
}
noInline(jazzy);

function foo(o) {
    let baz = hello();
    let jaz = jazzy();
    let v;
    try {
        v = o.f;
        v = o.f;
        v = o.f;
    } catch(e) {
        assert(baz === 20);
        assert(jaz === __jaz);
        assert(v === 2); // Really flagCount.
    }
    return v;
}
noInline(foo);

var objChain = {f: 40};
var fakeOut = {x: 30, f: 100};
for (let i = 0; i < 1000; i++)
    foo(i % 2 ? objChain : fakeOut);

var i;
var flag = "flag";
var flagCount = 0;
objChain = { 
    get f() {
        if (flagCount === 2)
            throw new Error("I'm testing you.");
        if (i === flag)
            flagCount++;
        return flagCount;
    }
};
for (i = 0; i < 100; i++) {
    allocate();
    if (i === 99)
        i = flag;
    foo(objChain);
}

fakeOut = {x: 30, get f() { return 100}};
for (i = 0; i < 100; i++) {
    allocate();
    if (i === 99)
        i = flag;
    foo(fakeOut);
}

var o = { 
    get f() {
        return flagCount;
    },
    x: 100
};

for (i = 0; i < 100; i++)
    foo(o);
