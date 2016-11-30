function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}
noInline(assert);

function foo(a, ...args) {
    let r = [...a, ...args];
    return r;
}
noInline(foo);

function escape(a) { return a; }
noInline(escape);
function bar(a, ...args) {
    escape(args);
    let r = [...a, ...args];
    return r;
}
noInline(foo);

for (let i = 0; i < 50000; i++) {
    for (let f of [foo, bar]) {
        let o = {};
        let a = [o, 20];
        let r = f(a, 30, 40);
        assert(r.length === 4);
        assert(r[0] === o);
        assert(r[1] === 20);
        assert(r[2] === 30);
        assert(r[3] === 40);
    }
}
