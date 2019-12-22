
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

{
    let f = test(foo, thisValue, 20, 30);
    for (let i = 0; i < 1000000; i++) {
        assert(f(foo, thisValue, 20, 30) === thisValue);
    }
}
{
    let f = test2(foo, thisValue);
    for (let i = 0; i < 1000000; i++) {
        assert(f(foo, thisValue, 20, 30) === thisValue);
    }
}
const verbose = false;
if (verbose)
    print(Date.now() - start);
