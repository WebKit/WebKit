
function assert(b) {
    if (!b)
        throw new Error("Bad")
}
noInline(assert);

function test(f, v, c, d) {
    return f.bind(v, c, d);
}
noInline(test);

function test2(f, v) {
    return f.bind(v);
}
noInline(test);

function foo(a,b,c,d,e,f) { return this; }
let thisValue = {};
let start = Date.now();
for (let i = 0; i < 1000000; i++) {
    let f = test(foo, thisValue, 20, 30);
    assert(f(foo, thisValue, 20, 30) === thisValue);
}
for (let i = 0; i < 1000000; i++) {
    let f = test2(foo, thisValue);
    assert(f(foo, thisValue, 20, 30) === thisValue);
}
const verbose = false;
if (verbose)
    print(Date.now() - start);
