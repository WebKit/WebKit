function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);

let x;
function foo() {
    return x;
}
for (var i = 0; i < 1000; i++) {
    assert(x === undefined);
    assert(foo() === undefined);
}
