function assert(b) {
    if (!b)
        throw new Error("bad assertion.");
}

function foo(o) {
    let r = [];
    for (let p in o)
        r.push(o[p]);
    return r;
}
noInline(foo);

let o = {};
o[Symbol()] = "symbol";
o.prop = "prop";
for (let i = 0; i < 1000; i++) {
    let arr = foo(o);
    assert(arr.length === 1);
    assert(arr[0] === "prop");
}

o.prop2 = "prop2";
for (let i = 0; i < 1000; i++) {
    let arr = foo(o);
    assert(arr.length === 2);
    assert(arr[0] === "prop");
    assert(arr[1] === "prop2");
}
