function foo(v, a, b) {
    if (v) {
        let r = a / b;
        OSRExit();
        return r;
    }
}
noInline(foo);

for (let i = 0; i < 10000; ++i) {
    let r = foo(true, 4, 4);
    if (r !== 1)
        throw new Error("Bad!");
}
if (foo(true, 1, 4) !== 0.25)
    throw new Error("Bad!");
